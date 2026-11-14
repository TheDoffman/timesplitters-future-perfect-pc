"""WSGI creator/moderator API. Public files stay in the separate approved snapshot."""
from http import HTTPStatus
import json
import os
import re
import sqlite3
from urllib.parse import parse_qs, urlsplit

from mod_accounts import Accounts, ServiceError, demand
from mod_package import ModError, read_json
from mod_repository import Repository
from mod_uploads import Uploads

MAX_JSON = 16384


class WorkshopAPI:
    def __init__(self, accounts, origin, local=False, uploads=None):
        parsed = urlsplit(origin)
        secure = parsed.scheme == 'https'
        preview = local and parsed.scheme == 'http' and parsed.hostname == '127.0.0.1'
        demand((secure or preview) and parsed.hostname and not parsed.username and not parsed.password
               and parsed.path in ('','/') and not parsed.query and not parsed.fragment,
               400, 'Configure an HTTPS workshop origin')
        self.origin = origin.rstrip('/')
        self.authority = parsed.netloc.lower()
        self.scheme = parsed.scheme
        self.accounts = accounts
        self.uploads = uploads if uploads is not None else Uploads(accounts)

    @staticmethod
    def _length(environ):
        value = environ.get('CONTENT_LENGTH', '')
        demand(isinstance(value, str) and re.fullmatch(r'0|[1-9][0-9]{0,8}', value), 411, 'Content-Length required')
        demand(not environ.get('HTTP_TRANSFER_ENCODING'), 400, 'Ambiguous request framing')
        return int(value)

    def _json(self, environ, keys):
        kind = environ.get('CONTENT_TYPE', '').lower().replace(' ', '')
        demand(kind in ('application/json','application/json;charset=utf-8'), 415, 'Use application/json')
        size = self._length(environ)
        demand(0 < size <= MAX_JSON, 413, 'Request body exceeds limit')
        data = environ['wsgi.input'].read(size)
        demand(isinstance(data, bytes) and len(data) == size, 400, 'Truncated request body')
        value = read_json(data)
        demand(isinstance(value, dict) and set(value) == set(keys), 400, 'Unknown or missing request fields')
        return value

    def _actor(self, environ):
        header = environ.get('HTTP_AUTHORIZATION', '')
        demand(isinstance(header, str) and header.startswith('Bearer '), 401, 'Authentication required')
        return self.accounts.authenticate(header[7:])

    @staticmethod
    def _identity(actor):
        return {'username': actor.username, 'role': actor.role}

    def dispatch(self, environ):
        # The production server trusts scheme headers only from its loopback
        # proxy. The application never trusts forwarded headers itself.
        demand(environ.get('wsgi.url_scheme') == self.scheme
               and environ.get('HTTP_HOST', '').lower() == self.authority, 403, 'Invalid workshop origin')
        supplied = environ.get('HTTP_ORIGIN')
        demand(supplied is None or supplied == self.origin, 403, 'Cross-origin request denied')
        demand(environ.get('HTTP_SEC_FETCH_SITE') != 'cross-site', 403, 'Cross-origin request denied')
        method, path = environ.get('REQUEST_METHOD'), environ.get('PATH_INFO','')
        demand(isinstance(path, str) and len(path) <= 256, 414, 'Request path exceeds limit')
        query = environ.get('QUERY_STRING','')
        cursor = 0
        if query:
            demand(method == 'GET' and path in ('/v1/moderation','/v1/uploads') and len(query) <= 64,
                   400, 'Unsupported query')
            try:
                parsed = parse_qs(query, strict_parsing=True, max_num_fields=1)
                demand(set(parsed) == {'after'} and len(parsed['after']) == 1
                       and re.fullmatch(r'0|[1-9][0-9]{0,8}', parsed['after'][0]), 400, 'Invalid page cursor')
                cursor = int(parsed['after'][0])
            except ValueError as exc:
                raise ServiceError(400, 'Invalid page cursor') from exc
        peer = environ.get('REMOTE_ADDR', '')
        if path == '/healthz' and method == 'GET':
            return 200, {'ready': True}
        if path == '/v1/accounts/redeem' and method == 'POST':
            data = self._json(environ, ('invitation','password'))
            result = self.accounts.redeem(data['invitation'], data['password'], peer)
            result['account'] = self._identity(self.accounts.authenticate(result['token']))
            return 201, result
        if path == '/v1/sessions' and method == 'POST':
            data = self._json(environ, ('username','password'))
            result = self.accounts.login(data['username'], data['password'], peer)
            result['account'] = self._identity(self.accounts.authenticate(result['token']))
            return 200, result
        actor = self._actor(environ)
        if path == '/v1/account' and method == 'GET':
            return 200, self._identity(actor)
        if path == '/v1/sessions/current' and method == 'DELETE':
            self.accounts.logout(actor)
            return 204, None
        if path == '/v1/account/password' and method == 'POST':
            data = self._json(environ, ('current','replacement'))
            self.accounts.change_password(actor, data['current'], data['replacement'], peer)
            return 204, None
        if path == '/v1/uploads' and method == 'POST':
            data = self._json(environ, ('id','version','sha256','size'))
            return 201, self.uploads.begin(actor, data['id'], data['version'], data['sha256'], data['size'])
        if path == '/v1/uploads' and method == 'GET':
            return 200, {'uploads': self.uploads.list(actor, cursor)}
        match = re.fullmatch(r'/v1/uploads/([0-9a-f]{32})', path)
        if match and method == 'GET':
            return 200, self.uploads.status(actor, match[1])
        if match and method == 'PUT':
            demand(environ.get('CONTENT_TYPE') == 'application/zip', 415, 'Use application/zip')
            return 202, self.uploads.receive(actor, match[1], environ['wsgi.input'], self._length(environ))
        if path == '/v1/moderation' and method == 'GET':
            return 200, {'uploads': self.uploads.reviews(actor, cursor)}
        match = re.fullmatch(r'/v1/moderation/([0-9a-f]{32})', path)
        if match and method == 'POST':
            data = self._json(environ, ('action','note'))
            return 200, self.uploads.moderate(actor, match[1], data['action'], data['note'])
        raise ServiceError(404, 'Route unavailable')

    def __call__(self, environ, start_response):
        try:
            status, value = self.dispatch(environ)
        except ServiceError as exc:
            status, value = exc.status, {'error': str(exc)}
        except ModError as exc:
            status, value = 400, {'error': str(exc)[:2000]}
        except (OSError, sqlite3.Error):
            status, value = 503, {'error': 'Workshop storage unavailable'}
        except Exception:
            status, value = 500, {'error': 'Workshop request failed'}
        body = b'' if value is None else json.dumps(value, ensure_ascii=True, separators=(',',':')).encode('utf-8')
        headers = [('Cache-Control','no-store'), ('X-Content-Type-Options','nosniff'),
                   ('Referrer-Policy','no-referrer'), ('X-Frame-Options','DENY')]
        if status != 204:
            headers += [('Content-Type','application/json; charset=utf-8'), ('Content-Length',str(len(body)))]
        if status == 429:
            headers.append(('Retry-After','300'))
        start_response(str(status) + ' ' + HTTPStatus(status).phrase, headers)
        return [body]


def create_app():
    """Gunicorn factory; repository/origin configuration contains no credentials."""
    root = os.environ['TSFP_WORKSHOP_REPOSITORY']
    origin = os.environ['TSFP_WORKSHOP_ORIGIN']
    repository = Repository(root)
    if os.name == 'posix':
        status = repository.root.stat()
        demand(status.st_uid == os.geteuid() and status.st_mode & 0o077 == 0,
               503, 'Production repository must be private to the service account')
    return WorkshopAPI(Accounts(repository), origin)
