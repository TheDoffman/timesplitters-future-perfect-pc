"""Loopback-only catalog preview. Public deployment needs the production service."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import tempfile
from urllib.error import HTTPError, URLError
from urllib.parse import parse_qs, urlsplit
from urllib.request import HTTPRedirectHandler, build_opener, ProxyHandler

from mod_package import MAX_ARCHIVE, ModError, require
from mod_repository import stage_install

HOME = b'''<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TimeSplitters Mod Workshop - local preview</title>
<style>body{background:#111b27;color:#ecf5fa;font:18px system-ui;max-width:900px;margin:50px auto;padding:20px}
input,button{font:inherit;padding:12px}input{width:65%}article{border:1px solid #456;padding:20px;margin:20px 0}
small{color:#adbccc}a{color:#65d8ee}</style>
<h1>TimeSplitters Mod Workshop</h1><p>Local repository preview. Packages install inactive; game integration is in development.</p>
<form id="search"><input id="query" aria-label="Search mods" placeholder="Search mods, creators, descriptions"><button>Search</button></form>
<p id="status" role="status"></p><main id="results"></main><script src="/app.js"></script></html>'''
APP = b'''
const form=document.querySelector('#search'), results=document.querySelector('#results'), status=document.querySelector('#status');
async function search(){
 results.replaceChildren();status.textContent='Searching...';
 try{const response=await fetch('/v1/catalog?q='+encodeURIComponent(document.querySelector('#query').value));
 if(!response.ok)throw Error('Search failed');const data=await response.json();
 status.textContent=data.releases.length+' approved release(s). Scanner results do not guarantee safety.';
 for(const release of data.releases){const m=release.manifest, article=document.createElement('article');
 for(const [tag,text] of [['h2',m.name+' '+m.version],['p',m.description],['p','By '+m.author],['small','Scanned by '+release.scan.engine+'; reviewed by repository operator']]){
 const e=document.createElement(tag);e.textContent=text;article.append(e);}
 const p=document.createElement('p'),a=document.createElement('a');a.href=release.download;a.textContent='Download package';p.append(a);article.append(p);
 const hash=document.createElement('p');hash.textContent='SHA-256: '+release.sha256;hash.style.overflowWrap='anywhere';article.append(hash);results.append(article);}}
 catch(e){status.textContent='Unable to search this repository.';}}
form.addEventListener('submit',e=>{e.preventDefault();search()});search();
'''


def make_server(repository, port=8765):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass  # Avoid retaining creator-controlled URLs in terminal logs.

        def reply(self, code, body, content_type):
            self.send_response(code)
            self.send_header('Content-Type', content_type)
            self.send_header('Content-Length', str(len(body)))
            self.send_header('X-Content-Type-Options', 'nosniff')
            self.send_header('Cache-Control', 'no-store')
            self.send_header('Content-Security-Policy', "default-src 'none'; script-src 'self'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'")
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            # Protect the local preview from DNS rebinding. No CORS or write API.
            expected_host = '127.0.0.1:' + str(self.server.server_port)
            if self.headers.get('Host') != expected_host:
                self.reply(403, b'Use the displayed loopback URL', 'text/plain')
                return
            try:
                url = urlsplit(self.path)
                require(len(self.path) <= 2048, 'Request too long')
                if url.path == '/':
                    self.reply(200, HOME, 'text/html; charset=utf-8')
                elif url.path == '/app.js':
                    self.reply(200, APP, 'text/javascript; charset=utf-8')
                elif url.path == '/v1/catalog':
                    query = parse_qs(url.query).get('q', [''])[0]
                    data = {'schema': 1, 'releases': repository.catalog(query)}
                    self.reply(200, json.dumps(data).encode(), 'application/json')
                elif url.path.startswith('/v1/packages/'):
                    sha = url.path.removeprefix('/v1/packages/')
                    data = repository.approved_blob(sha).read_bytes()
                    self.reply(200, data, 'application/zip')
                else:
                    self.reply(404, b'Not found', 'text/plain')
            except (ModError, OSError):
                self.reply(404, b'Release unavailable or request invalid', 'text/plain')

    server = ThreadingHTTPServer(('127.0.0.1', port), Handler)
    server.daemon_threads = True
    return server


class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, *_args, **_kwargs):
        raise ModError('Repository redirects are forbidden')


def fetch_install(server, sha, mods_root):
    """Local preview client. Public clients require signed metadata/TLS first."""
    url = urlsplit(server)
    require(url.scheme == 'http' and url.hostname == '127.0.0.1' and url.port
            and not url.username and not url.password and url.path in ('', '/')
            and not url.query and not url.fragment, 'Use the local preview http://127.0.0.1:PORT URL')
    from mod_package import SHA
    require(isinstance(sha, str) and SHA.fullmatch(sha), 'Invalid release digest')
    opener = build_opener(ProxyHandler({}), NoRedirect())
    try:
        with tempfile.TemporaryDirectory() as temporary:
            package = Path(temporary) / 'package.zip'
            with opener.open(server.rstrip('/') + '/v1/packages/' + sha, timeout=30) as response:
                require(response.status == 200, 'Download failed')
                with package.open('xb') as output:
                    count = 0
                    while True:
                        block = response.read(64 * 1024)
                        if not block:
                            break
                        count += len(block)
                        require(count <= MAX_ARCHIVE, 'Download exceeds limit')
                        output.write(block)
            return stage_install(package, sha, mods_root)
    except (HTTPError, URLError, TimeoutError) as exc:
        raise ModError('Repository download unavailable') from exc
