"""Record every ELF provider and ship exact distro source packages with the preview."""
import hashlib,json,re,shutil,subprocess
from pathlib import Path

def digest(path):
    with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def package_owner(path):
    for candidate in (str(path),str(path).removeprefix('/usr')):
        r=subprocess.run(['dpkg-query','-S',candidate],capture_output=True,text=True)
        if r.returncode==0:
            owners={line.split(': /',1)[0] for line in r.stdout.splitlines() if ': /' in line}
            if len(owners)==1:return owners.pop()
    raise ValueError('No unique distro owner for shipped ELF: '+str(path))

def source_identity(owner):
    value=subprocess.check_output(['dpkg-query','-W','-f=${source:Package}\t${source:Version}',owner],text=True).strip().split('\t')
    if len(value)!=2 or not re.fullmatch(r'[a-z0-9][a-z0-9+.-]*',value[0]) or not re.fullmatch(r'[A-Za-z0-9:.+~_-]+',value[1]):
        raise ValueError('Invalid distro source identity: '+owner)
    return value

def source_files(text,version):
    """Use hashes from the authenticated APT source index, not cache file size."""
    for paragraph in text.split('\n\n'):
        fields={};key=None
        for line in paragraph.splitlines():
            if line.startswith((' ','\t')) and key:fields[key]+='\n'+line.strip()
            elif ': ' in line:key,value=line.split(': ',1);fields[key]=value
            elif line.endswith(':'):key=line[:-1];fields[key]=''
        if fields.get('Version')!=version:continue
        result={}
        for line in fields.get('Checksums-Sha256','').splitlines():
            if not line.strip():continue
            sha,size,name=line.split()
            if not re.fullmatch('[0-9a-f]{64}',sha) or not size.isdigit() or Path(name).name!=name or name in ('.','..'):
                raise ValueError('Unsafe source index entry')
            result[name]=(sha,int(size))
        if result:return result
    raise ValueError('Exact installed source version is unavailable from the authenticated archive: '+version)

def apt_options(root):
    base=root/'external/linux-package-sources/apt';lists=base/'lists';(lists/'partial').mkdir(parents=True,exist_ok=True)
    source=base/'sources.sources'
    original=Path('/etc/apt/sources.list.d/ubuntu.sources')
    if not original.is_file():raise ValueError('Source collection currently requires Ubuntu deb822 archive configuration')
    text=original.read_text().replace('Types: deb\n','Types: deb-src\n')
    if 'Types: deb-src' not in text:raise ValueError('Missing Ubuntu source repositories')
    if not source.exists() or source.read_text()!=text:source.write_text(text)
    options=['-o','Dir::Etc::sourcelist='+str(source),'-o','Dir::Etc::sourceparts=-','-o','Dir::State::lists='+str(lists)]
    if not (base/'updated').exists():
        subprocess.run(['apt-get',*options,'update'],check=True)
        (base/'updated').write_text('Authenticated Ubuntu source indexes\n')
    return options

def collect(root,stage,records,launcher,media,sdk,python):
    """Fail closed for unclassified libraries or missing notices/source archives."""
    root,stage,launcher=Path(root),Path(stage),Path(launcher).resolve()
    components={};sources={};options=None
    for target,record in records.items():
        origin=Path(record['source']).resolve()
        common=dict(sha256=record['source_sha256'])
        if origin.is_relative_to(media.resolve()):
            components[target]=dict(common,provider='FFmpeg',source='Source/FFmpeg/pin.json');continue
        if origin.is_relative_to(sdk.resolve()):
            if origin.name!='libdxcompiler.so':raise ValueError('Unclassified SDK binary: '+origin.name)
            components[target]=dict(common,provider='DXC',notices=['Licenses/DXC.txt','Licenses/DXC-ThirdParty.txt']);continue
        if origin==root/'build/linux/game/futureperfect_recomp':
            components[target]=dict(common,provider='project',source='Source/pc-port-source.zip');continue
        if origin.is_relative_to(launcher):
            components[target]=dict(common,provider='launcher',notices='Support/Launcher/Licenses');continue
        if target.startswith('Support/Python/') and '/site-packages/' in target:
            components[target]=dict(common,provider='pinned Python wheels',wheels=python['wheels']);continue
        owner=package_owner(origin);name,version=source_identity(owner);key=name+'='+version
        copyright=Path('/usr/share/doc')/owner.split(':')[0]/'copyright'
        if not copyright.is_file():raise ValueError('Missing distro copyright: '+owner)
        notice=stage/'Licenses/System'/name/'copyright';notice.parent.mkdir(parents=True,exist_ok=True)
        if notice.exists() and digest(notice)!=digest(copyright):
            notice=notice.with_name(owner.replace(':','-')+'-copyright')
        shutil.copy2(copyright,notice)
        if key not in sources:
            cache=root/'external/linux-package-sources'/name/version.replace(':','_')
            cache.mkdir(parents=True,exist_ok=True)
            if options is None:options=apt_options(root)
            subprocess.run(['apt-get',*options,'source','--download-only','--only-source',key],cwd=cache,check=True)
            metadata=subprocess.check_output(['apt-cache',*options,'showsrc','--only-source',name],text=True)
            expected=source_files(metadata,version)
            if not any(n.endswith('.dsc') for n in expected):raise ValueError('Missing exact distro source descriptor: '+key)
            # apt validates archives against the signed source index. Retain every
            # downloaded component, including Debian patches and build recipes.
            files={}
            for filename,(sha,size) in sorted(expected.items()):
                path=cache/filename
                if path.is_symlink() or not path.is_file() or path.stat().st_size!=size or digest(path)!=sha:raise ValueError('Source archive checksum mismatch: '+key+'/'+filename)
                relative='Source/System/'+name+'/'+path.name;dest=stage/relative
                dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,dest);files[relative]=digest(dest)
            if len(files)<2:raise ValueError('Incomplete corresponding source package: '+key)
            sources[key]=dict(package=name,version=version,files=files)
        components[target]=dict(common,provider='Ubuntu',binary_package=owner,source_package=key,notice=notice.relative_to(stage).as_posix())
    for path in Path('/usr/share/common-licenses').iterdir():
        if path.is_file():dest=stage/'Licenses/Common'/path.name;dest.parent.mkdir(exist_ok=True);shutil.copy2(path,dest)
    report=dict(schema=1,components=components,source_packages=sources)
    (stage/'Support/Runtime/dependency-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    return report
