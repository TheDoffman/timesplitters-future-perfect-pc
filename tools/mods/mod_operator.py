"""Local operator actions; invitation secrets are written only to private, new files."""
import json
import os
import secrets

from mod_accounts import demand


def secret_directory(repository):
    demand(os.name == 'posix', 503, 'Issue invitations on the private Mac workshop host')
    root = repository.root
    demand(root.stat().st_uid == os.geteuid() and root.stat().st_mode & 0o077 == 0,
           503, 'Repository must be private to the operator account (mode0700)')
    folder = root / 'operator-secrets'
    demand(not folder.is_symlink(), 503, 'Private secrets directory is linked')
    folder.mkdir(mode=0o700, exist_ok=True)
    status = folder.stat()
    demand(status.st_uid == os.geteuid() and status.st_mode & 0o077 == 0,
           503, 'Secrets directory must be private to the operator account')
    return folder


def issue_invitation(accounts, name, role='creator', recovery_note=None):
    folder = secret_directory(accounts.repository)
    secret = (accounts.create_invite(name, role) if recovery_note is None
              else accounts.recovery_invite(name, recovery_note))
    destination = folder / (name + '-' + secrets.token_hex(8) + '.json')
    created = False
    try:
        descriptor = os.open(destination, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
        created = True
        with os.fdopen(descriptor, 'w', encoding='utf-8', newline='\n') as output:
            json.dump(dict(schema=1, username=name, invitation=secret,
                           expires=accounts.now() + 86400), output)
            output.write('\n')
            output.flush()
            os.fsync(output.fileno())
    except BaseException:
        accounts.cancel_invite(secret)
        if created:
            destination.unlink()
        raise
    return {'username': name, 'secret_file': str(destination), 'expires': accounts.now() + 86400}
