// Optional local-router adapter. Copy beside supervisor-session.mjs/router.mjs.
// Never execute workspace code or read arbitrary paths from its manifest.
import fs from 'node:fs';
import path from 'node:path';

const prefix='docs/agent-context/';
const topicIds=['runtime','launcher','systems','build-test','investigation','mod-workshop'];
const files=new Set(['manifest.json','README.md',...topicIds.map(id=>id+'.md')].map(n=>prefix+n));
const maxBytes=6500;
function bounded(text,limit=maxBytes){
  if(Buffer.byteLength(text)<=limit)return text;
  return Buffer.from(text).subarray(0,limit-64).toString('utf8').replace(/\uFFFD$/,'')+'\n[Context excerpt truncated]';
}
function read(root,name){
  if(!files.has(name))throw Error('Context file is not allowlisted');
  let current=root;
  for(const segment of name.split('/')){
    current=path.join(current,segment);
    if(fs.lstatSync(current).isSymbolicLink())throw Error('Redirected context file');
  }
  const real=fs.realpathSync(current),relative=path.relative(root,real);
  if(relative.startsWith('..')||path.isAbsolute(relative))throw Error('Context file escapes repository');
  const stat=fs.statSync(current);
  if(!stat.isFile()||stat.size>65536)throw Error('Invalid context file size');
  return fs.readFileSync(current,'utf8');
}
function excerpt(root,name){
  const text=read(root,name),match=text.match(/<!-- packet:start -->\s*([\s\S]*?)\s*<!-- packet:end -->/);
  if(!match||Buffer.byteLength(match[1])>3000)throw Error('Missing/oversized context excerpt');
  return match[1];
}
function rootFor(cwd){
  if(typeof cwd!=='string'||!cwd)return null;
  let current=path.resolve(cwd);
  for(let i=0;i<16;i++){
    if(fs.existsSync(path.join(current,'AGENTS.md'))&&fs.existsSync(path.join(current,'.git')))return current;
    const parent=path.dirname(current);if(parent===current)break;current=parent;
  }
  return null;
}
export function readRepositoryContext(cwd,query=''){
  try{
    const candidate=rootFor(cwd);
    if(!candidate)return {text:'',files:[],bytes:0,status:'absent'};
    if(fs.lstatSync(candidate).isSymbolicLink())throw Error('Redirected repository root');
    const root=fs.realpathSync(candidate),manifestPath=path.join(root,prefix,'manifest.json');
    if(!fs.existsSync(manifestPath))return {text:'',files:[],bytes:0,status:'absent'};
    const manifest=JSON.parse(read(root,prefix+'manifest.json'));
    if(manifest.version!==1||manifest.overview!==prefix+'README.md'||!Array.isArray(manifest.topics)||manifest.topics.length>topicIds.length)throw Error('Invalid context manifest');
    const ids=new Set();
    const topics=manifest.topics.map((topic,i)=>{
      if(!topicIds.includes(topic.id)||ids.has(topic.id)||topic.file!==prefix+topic.id+'.md'||!Array.isArray(topic.keywords)||topic.keywords.length>40)throw Error('Invalid context topic');
      ids.add(topic.id);
      const score=topic.keywords.reduce((total,keyword)=>{
        if(typeof keyword!=='string'||keyword.length>64)throw Error('Invalid context keyword');
        const escaped=keyword.replace(/[.*+?^${}()|[\]\\]/g,'\\$&');
        return total+Number(new RegExp('(?<!\\w)'+escaped+'(?:s|ing)?(?!\\w)','i').test(String(query).slice(-24000)));
      },0);
      return {file:topic.file,score,index:i};
    }).sort((a,b)=>b.score-a.score||a.index-b.index);
    const names=[manifest.overview,...topics.filter(t=>t.score>0).slice(0,2).map(t=>t.file)];
    const text=bounded(names.map(name=>'Reference: '+name+'\n'+excerpt(root,name)).join('\n\n'));
    return {text,files:names,bytes:Buffer.byteLength(text),status:'loaded'};
  }catch{
    // A malformed/redirected guide must never break the native panel or leak
    // raw filesystem errors. The worker can report/check missing context locally.
    return {text:'',files:[],bytes:0,status:'invalid'};
  }
}
