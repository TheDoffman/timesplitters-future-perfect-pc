// Bounded, read-only evidence broker. Never execute model-supplied commands or
// workspace code. Only Git-indexed public source and the pinned runtime qualify.
import fs from 'node:fs';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';

export const evidenceLimits=Object.freeze({requestsPerRound:3,rounds:2,totalBytes:6500,fileBytes:2*1024*1024,scanBytes:8*1024*1024,scanFiles:400,lines:80});
const sha=data=>createHash('sha256').update(data).digest('hex');
function git(root,...args){return execFileSync('git',['-C',root,'-c','core.fsmonitor=false',...args],{encoding:'utf8',windowsHide:true,timeout:5000,maxBuffer:4*1024*1024,stdio:['ignore','pipe','pipe']});}
function relative(name){
  if(typeof name!=='string'||!name||name.length>240||name.includes('\\')||name.includes(':')||/[\x00-\x1f\x7f]/.test(name)||name.startsWith('/')||name.split('/').some((p,i)=>!p||p==='.'||p==='..'||p.startsWith('.')&&!(i===0&&p==='.github')))throw Error('path_not_allowed');
  return name;
}
function sourceAllowed(name,namespace){
  try{relative(name);}catch{return false;}
  if(name.split('/').some(p=>/^(?:private|assets|build|logs|gen|generated|node_modules|vendor|third_party)$/i.test(p)||/(?:^|[._-])(?:auth|credentials?|secrets?|tokens?|passwords?)(?:[._-]|$)/i.test(p)))return false;
  if(namespace==='runtime')return /^(?:CMakeLists\.txt$|src\/|tools\/recomp\/|scripts\/|templates\/|config\/extra_symbols\/|titles\/futureperfect\/(?:src\/|CMakeLists\.txt$))/.test(name)&&(/\.(?:c|h|cpp|hpp|inc|py|json|txt)$/.test(name)||name.endsWith('CMakeLists.txt'));
  return /^(?:src\/|tests\/|tools\/|patches\/|docs\/)/.test(name)&&(/\.(?:c|h|cpp|hpp|m|mm|inc|def|cs|xaml|py|mjs|js|cjs|ps1|sh|cmd|command|patch|md|json|txt|cmake|hlsl|glsl|metal|vert|frag)$/.test(name)||name.endsWith('CMakeLists.txt'))
    || /^(?:tsfp\.ps1|setup\.ps1|requirements(?:-release)?\.txt|(?:README|AGENTS|STATUS|MAC_PORT_BACKLOG|CONTRIBUTING)\.md|Play(?:-Native-Mac)?\.command|Play(?:-Quiet|-Textures)?\.cmd|\.github\/(?:workflows|ISSUE_TEMPLATE)\/[^/]+\.ya?ml|config\/(?:[^/]+\.default\.(?:json|ini)|vulkan-sdk\.json|mac-launcher-sdk\.json|native-mac-runtime-licenses\.json|texture-pack\.json))$/.test(name);
}
function safePath(root,name){
  relative(name);let current=root;
  for(const segment of name.split('/')){current=path.join(current,segment);if(fs.lstatSync(current).isSymbolicLink())throw Error('redirected_path');}
  const real=fs.realpathSync(current),rel=path.relative(root,real);
  if(rel==='..'||rel.startsWith('..'+path.sep)||path.isAbsolute(rel))throw Error('redirected_path');
  return current;
}
export function redactSource(text){
  return String(text).replace(/\b(?:sk-[A-Za-z0-9_-]{16,}|gh[pousr]_[A-Za-z0-9_]{15,}|github_pat_[A-Za-z0-9_]{15,})\b/g,'[REDACTED]')
    .replace(/\bBearer\s+[A-Za-z0-9._~+\/-]{12,}/gi,'Bearer [REDACTED]')
    .replace(/((?:password|api[_-]?key|access[_-]?token|refresh[_-]?token|client[_-]?secret)\s*[:=]\s*["']?)[^\s"';,}]+/gi,'$1[REDACTED]');
}
function clipped(text,max){if(Buffer.byteLength(text)<=max)return text;return Buffer.from(text).subarray(0,Math.max(0,max-40)).toString('utf8').replace(/\uFFFD$/,'')+'\n[excerpt truncated]';}
function fit(result,maxBytes){
  while(Buffer.byteLength(JSON.stringify(result))>maxBytes){
    if(result.text&&Buffer.byteLength(result.text)>100){result.text=clipped(result.text,Math.floor(Buffer.byteLength(result.text)/2));result.truncated=true;}
    else if(result.hits?.length){result.hits.pop();result.truncated=true;}
    else return {status:'unavailable',reason:'evidence_byte_budget'};
  }
  return result;
}
function entries(root){
  const result=new Map();
  for(const row of git(root,'ls-files','--stage','-z').split('\0')){
    const m=/^(\d+) ([a-f0-9]{40,64}) (\d)\t(.+)$/.exec(row);
    if(m&&m[3]==='0'&&['100644','100755'].includes(m[1]))result.set(m[4],m[2]);
  }
  return result;
}
function revision(root){const value=git(root,'rev-parse','HEAD').trim();if(!/^[a-f0-9]{40,64}$/.test(value))throw Error('revision_unavailable');return value;}
function requestShape(request){
  if(!request||typeof request!=='object'||Array.isArray(request))throw Error('invalid_request');
  const keys={read:['kind','namespace','path','start','lines','expectedSha256'],search:['kind','namespace','query','scope'],references:['kind','namespace','symbol','scope'],patches:['kind','query']}[request.kind];
  if(!keys||Object.keys(request).some(k=>!keys.includes(k)))throw Error('invalid_request');
  if(request.kind!=='patches'&&!['repository','runtime'].includes(request.namespace))throw Error('invalid_namespace');
  if(request.kind==='read'){
    relative(request.path);
    if(!Number.isInteger(request.start)||request.start<1||request.start>1000000||!Number.isInteger(request.lines)||request.lines<1||request.lines>evidenceLimits.lines)throw Error('invalid_range');
    if(request.expectedSha256!==undefined&&!/^[a-f0-9]{64}$/.test(request.expectedSha256))throw Error('invalid_hash');
  }else{
    const query=request.kind==='references'?request.symbol:request.query;
    if(typeof query!=='string'||query.length<3||query.length>120||/[\r\n\0]/.test(query)||request.kind==='references'&&!/^[A-Za-z_]\w*$/.test(query))throw Error('invalid_query');
    if(request.scope!==undefined)relative(request.scope);
  }
  return request;
}
export function validateEvidenceRequest(request){try{requestShape(request);return true;}catch{return false;}}

export function createRepositoryEvidence(cwd){
  let root,rootRevision,rootFiles,runtime;const observed=new Map();
  try{
    root=fs.realpathSync(git(path.resolve(cwd),'rev-parse','--show-toplevel').trim());
    rootRevision=revision(root);rootFiles=entries(root);
  }catch{return {retrieve:()=>({status:'unavailable',reason:'repository_revision_unavailable'}),revision:null};}
  function space(namespace){
    if(namespace==='repository')return {root,revision:rootRevision,files:rootFiles};
    if(runtime)return runtime;
    const dir=safePath(root,'external/ts3FPrecomp');
    if(!rootFiles.has('tsfp.ps1'))throw Error('runtime_pin_unavailable');
    const pin=git(root,'cat-file','blob',rootFiles.get('tsfp.ps1')).match(/\$revision = '([a-f0-9]{40})'/)?.[1];
    const actual=revision(dir);
    if(!pin||actual!==pin)throw Error('runtime_revision_mismatch');
    runtime={root:fs.realpathSync(dir),revision:actual,files:entries(dir)};return runtime;
  }
  function read(space,name,compareIndex=false,budget=evidenceLimits.fileBytes){
    if(!sourceAllowed(name,space===runtime?'runtime':'repository')||!space.files.has(name))throw Error('file_not_allowlisted');
    const file=safePath(space.root,name),before=fs.statSync(file);
    if(!before.isFile()||before.size>evidenceLimits.fileBytes)throw Error('file_too_large');
    if(before.size>budget)throw Error('scan_budget');
    if(before.nlink!==1)throw Error('redirected_path');
    const bytes=fs.readFileSync(file),after=fs.statSync(file);
    if(after.mtimeMs!==before.mtimeMs||after.size!==before.size)throw Error('file_changed_during_read');
    const text=new TextDecoder('utf-8',{fatal:true}).decode(bytes);
    if(text.includes('\0'))throw Error('binary_file');
    const object=space.files.get(name),indexed=compareIndex?git(space.root,'cat-file','blob',object):null;
    return {text,meta:{namespace:space===runtime?'runtime':'repository',path:name,revision:space.revision,sha256:sha(bytes),indexBlob:object,workingTreeDiffers:indexed===null?null:text.replace(/\r\n/g,'\n')!==indexed.replace(/\r\n/g,'\n')}};
  }
  function retrieve(input,{maxBytes=3000}={}){
    try{
      const request=requestShape(input);
      if(revision(root)!==rootRevision)throw Error('repository_revision_changed');
      const source=space(request.kind==='patches'?'repository':request.namespace);
      if(revision(source.root)!==source.revision)throw Error('runtime_revision_changed');
      const base={status:'ok',repositoryRevision:rootRevision};
      if(request.kind==='read'){
        const item=read(source,request.path,true);observed.set(source.root+'\0'+request.path,{root:source.root,path:request.path,sha256:item.meta.sha256});
        if(request.expectedSha256&&request.expectedSha256!==item.meta.sha256)throw Error('file_hash_changed');
        const lines=item.text.split(/\r?\n/),selected=lines.slice(request.start-1,request.start-1+request.lines);
        if(!selected.length)throw Error('range_out_of_bounds');
        const text=clipped(redactSource(selected.map((line,i)=>(request.start+i)+': '+line).join('\n')),Math.max(100,maxBytes-700));
        return fit({...base,source:item.meta,start:request.start,end:request.start+selected.length-1,text,truncated:text.endsWith('[excerpt truncated]')},maxBytes);
      }
      const query=request.kind==='references'?request.symbol:request.query;
      let candidates=[...source.files.keys()].filter(name=>sourceAllowed(name,source===runtime?'runtime':'repository')&&(!request.scope||name===request.scope||name.startsWith(request.scope+'/'))).sort();
      let patchIndex;
      if(request.kind==='patches'){
        candidates=candidates.filter(name=>/^patches\/.+\.patch$/.test(name)&&!name.split('/').includes('legacy'));
        try{
          patchIndex=JSON.parse(read(source,'docs/agent-context/patch-index.json').text);
          const loader=read(source,'tools/apply-patches.py').text.replace(/\r\n/g,'\n');
          if(patchIndex.loaderSha256!==sha(loader))throw Error('stale_index');
          const order=new Map(patchIndex.patches.map((p,i)=>[p.path??'patches/'+p.name,i]));candidates.sort((a,b)=>(order.get(b)??-1)-(order.get(a)??-1));
        }catch{patchIndex=null;}
      }
      let scannedFiles=0,scannedBytes=0,skippedFiles=0,truncated=false;const hits=[];
      const needle=query.toLowerCase();
      const identifier=request.kind==='references'?new RegExp('(?<![A-Za-z0-9_])'+query+'(?![A-Za-z0-9_])'):null;
      outer:for(const name of candidates){
        if(scannedFiles>=evidenceLimits.scanFiles||scannedBytes>=evidenceLimits.scanBytes){truncated=true;break;}
        let item;try{item=read(source,name,false,evidenceLimits.scanBytes-scannedBytes);}catch{skippedFiles++;truncated=true;continue;}
        scannedFiles++;scannedBytes+=Buffer.byteLength(item.text);
        const lines=item.text.split(/\r?\n/);let target='';
        for(let i=0;i<lines.length;i++){
          const line=lines[i];if(line.startsWith('--- a/')||line.startsWith('+++ b/'))target=line.slice(6);
          if(identifier?!identifier.test(line):!line.toLowerCase().includes(needle))continue;
          const hit={...item.meta,line:i+1,text:clipped(redactSource(line),240)};
          if(request.kind==='patches'){
            hit.target=target;hit.removed=line.startsWith('-')&&!line.startsWith('---');
            const record=patchIndex?.patches?.find(p=>p.name===path.posix.basename(name));
            hit.order=record&&record.sha256===item.meta.sha256?patchIndex.patches.indexOf(record)+1:null;
          }
          if(hits.length>=8||Buffer.byteLength(JSON.stringify({...base,hits:[...hits,hit],scannedFiles,scannedBytes,truncated:true}))+100>maxBytes){truncated=true;break outer;}
          hits.push(hit);
          observed.set(source.root+'\0'+name,{root:source.root,path:name,sha256:item.meta.sha256});
        }
      }
      return fit({...base,kind:request.kind,query,hits,scannedFiles,scannedBytes,skippedFiles,truncated,note:request.kind==='references'?'Lexical references, not a verified call graph.':request.kind==='patches'?'Indexed active patches only; later hunks can supersede earlier ones.':undefined},maxBytes);
    }catch(error){
      const allowed=['path_not_allowed','invalid_request','invalid_namespace','invalid_range','invalid_hash','invalid_query','redirected_path','runtime_pin_unavailable','runtime_revision_mismatch','file_not_allowlisted','file_too_large','file_changed_during_read','binary_file','repository_revision_changed','runtime_revision_changed','file_hash_changed','range_out_of_bounds'];
      return {status:'unavailable',reason:allowed.includes(error.message)?error.message:'evidence_unavailable',repositoryRevision:rootRevision};
    }
  }
  function assertFresh(){
    if(revision(root)!==rootRevision||runtime&&revision(runtime.root)!==runtime.revision)throw Error('Evidence revision changed');
    for(const item of observed.values()){
      const file=safePath(item.root,item.path),stat=fs.statSync(file);
      if(stat.size>evidenceLimits.fileBytes||stat.nlink!==1||sha(fs.readFileSync(file))!==item.sha256)throw Error('Evidence file changed; refresh locally');
    }
  }
  return {retrieve,assertFresh,revision:rootRevision};
}
