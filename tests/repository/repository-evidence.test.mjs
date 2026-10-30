import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {createRepositoryEvidence,evidenceLimits} from '../../tools/router/repository-evidence.mjs';
import {parseEvidenceReply,runEvidenceAdvisory} from '../../tools/router/advisory-evidence.mjs';

function git(root,...args){return execFileSync('git',['-C',root,...args],{encoding:'utf8',windowsHide:true,stdio:['ignore','pipe','pipe']});}
function save(root,name,text){const file=path.join(root,name);fs.mkdirSync(path.dirname(file),{recursive:true});fs.writeFileSync(file,text);}
function commit(root){git(root,'add','.');git(root,'-c','user.name=Fixture','-c','user.email=fixture@example.invalid','commit','-qm','Fixture');}
function fixture(t){
  const parent=fs.realpathSync(os.tmpdir()),root=fs.mkdtempSync(path.join(parent,'repo-evidence-'));
  t.after(()=>{assert.equal(path.dirname(root),parent);assert.match(path.basename(root),/^repo-evidence-[\w-]+$/);assert.equal(fs.realpathSync(root),root);fs.rmSync(root,{recursive:true,force:true});});
  git(root,'init','-q');save(root,'src/counter.c','int count(int n) { return n + 1; }\nint caller(void) { return count(3); }\n');commit(root);return root;
}
const read={kind:'read',namespace:'repository',path:'src/counter.c',start:1,lines:2};
const need=requests=>JSON.stringify({kind:'need_evidence',question:'Which operation actually computes this value?',requests});

test('read carries exact revision, line range, content hash and working-tree provenance',t=>{
 const root=fixture(t),broker=createRepositoryEvidence(root),a=broker.retrieve(read);
 assert.equal(a.status,'ok');assert.equal(a.source.revision,git(root,'rev-parse','HEAD').trim());assert.equal(a.start,1);assert.equal(a.end,2);assert.equal(a.source.workingTreeDiffers,false);assert.match(a.source.sha256,/^[a-f0-9]{64}$/);assert.match(a.text,/1: int count/);
 save(root,'src/counter.c','int count(int n) { return n * 2; }\n');assert.throws(()=>broker.assertFresh(),/changed/);
 const b=createRepositoryEvidence(root).retrieve({...read,expectedSha256:a.source.sha256});assert.equal(b.reason,'file_hash_changed');
 assert.equal(createRepositoryEvidence(root).retrieve(read).source.workingTreeDiffers,true);
});
test('source search is literal, bounded and returns lexical caller locations',t=>{
 const root=fixture(t);save(root,'src/counter.c','int count(int n) { return n + 1; }\nint caller(void) { return count(3); }\nint discount = 0; int Count = 1;\n');const broker=createRepositoryEvidence(root);
 const result=broker.retrieve({kind:'references',namespace:'repository',symbol:'count',scope:'src'});
 assert.equal(result.hits.length,2);assert.equal(result.hits[1].line,2);assert.match(result.note,/not a verified call graph/);
 assert.equal(broker.retrieve({kind:'search',namespace:'repository',query:'.*not-a-regex'}).hits.length,0);
});
test('private, generated, credentials, untracked and command-shaped requests cannot read files',t=>{
 const root=fixture(t);save(root,'src/untracked.c','DO_NOT_SEND');
 for(const name of ['private/data.c','src/auth.json','src/recomp/gen/guest.c','logs/output.c'])save(root,name,'DO_NOT_SEND');
 commit(root);save(root,'src/extra.c','DO_NOT_SEND');
 const broker=createRepositoryEvidence(root);
 for(const name of ['../src/counter.c','C:/secret','src\\counter.c','private/data.c','src/auth.json','src/recomp/gen/guest.c','logs/output.c','src/extra.c'])assert.notEqual(broker.retrieve({...read,path:name}).status,'ok',name);
 assert.equal(broker.retrieve({...read,command:'whoami'}).reason,'invalid_request');
});
test('secret-shaped literals are redacted before transmission',t=>{
 const root=fixture(t),secret='ghp_'+ 'a'.repeat(32);save(root,'src/counter.c',`const char *api_key = "${secret}";\n`);
 const result=createRepositoryEvidence(root).retrieve(read);assert.ok(!JSON.stringify(result).includes(secret));assert.match(result.text,/REDACTED/);
});
test('public build docs/defaults qualify while personal configuration stays excluded',t=>{
 const root=fixture(t);for(const name of ['docs/development/building.md','config/pc-controls.default.json','.github/workflows/build.yml','config/pc-display.ini'])save(root,name,'public fixture text\n');commit(root);
 const broker=createRepositoryEvidence(root);
 for(const name of ['docs/development/building.md','config/pc-controls.default.json','.github/workflows/build.yml'])assert.equal(broker.retrieve({...read,path:name}).status,'ok',name);
 assert.equal(broker.retrieve({...read,path:'config/pc-display.ini'}).reason,'file_not_allowlisted');
});
test('UTF-8 and JSON escaping respect the requested byte budget',t=>{
 const root=fixture(t);save(root,'src/counter.c',('\\"\u754c'.repeat(600)+'\n').repeat(80));
 const result=createRepositoryEvidence(root).retrieve({...read,lines:80},{maxBytes:1500});assert.ok(Buffer.byteLength(JSON.stringify(result))<=1500);assert.equal(result.truncated,true);
});
test('root revision changes invalidate an existing evidence broker',t=>{
 const root=fixture(t),broker=createRepositoryEvidence(root);save(root,'src/new.c','new');commit(root);assert.equal(broker.retrieve(read).reason,'repository_revision_changed');
});
test('hardlinked source is not an alternate route to outside content',t=>{
 const root=fixture(t),outside=fixture(t);fs.unlinkSync(path.join(root,'src/counter.c'));fs.linkSync(path.join(outside,'src/counter.c'),path.join(root,'src/counter.c'));
 assert.equal(createRepositoryEvidence(root).retrieve(read).reason,'redirected_path');
});
test('patch lookup exposes effective order only for a current loader/index/patch',t=>{
 const root=fixture(t);save(root,'tools/build/apply-patches.py','# fixture loader\n');save(root,'patches/fix.patch','--- a/src/example.c\n+++ b/src/example.c\n@@ -1 +1 @@\n-old_gate();\n+new_gate();\n');
 const hash=text=>createHash('sha256').update(text).digest('hex');
 save(root,'docs/agent-context/patch-index.json',JSON.stringify({patches:[{name:'fix.patch',sha256:hash(fs.readFileSync(path.join(root,'patches/fix.patch')))}],loaderSha256:hash('# fixture loader\n')}));commit(root);
 let result=createRepositoryEvidence(root).retrieve({kind:'patches',query:'new_gate'});assert.equal(result.hits[0].order,1);assert.equal(result.hits[0].target,'src/example.c');
 save(root,'tools/build/apply-patches.py','# modified loader\n');result=createRepositoryEvidence(root).retrieve({kind:'patches',query:'new_gate'});assert.equal(result.hits[0].order,null);
});
test('junction traversal is rejected even for a formerly indexed source path',t=>{
 const root=fixture(t),outside=fixture(t);save(root,'src/nested/file.c','indexed');commit(root);
 fs.unlinkSync(path.join(root,'src/nested/file.c'));fs.rmdirSync(path.join(root,'src/nested'));fs.symlinkSync(path.join(outside,'src'),path.join(root,'src/nested'),process.platform==='win32'?'junction':'dir');
 assert.equal(createRepositoryEvidence(root).retrieve({...read,path:'src/nested/counter.c'}).reason,'file_not_allowlisted');
 save(outside,'src/file.c','PRIVATE');assert.equal(createRepositoryEvidence(root).retrieve({...read,path:'src/nested/file.c'}).reason,'redirected_path');
});
test('runtime retrieval requires the pinned upstream revision and excludes generated code',t=>{
 const root=fixture(t),runtime=path.join(root,'external/ts3FPrecomp');fs.mkdirSync(runtime,{recursive:true});git(runtime,'init','-q');save(runtime,'src/kernel/gate.c','void release_gate(void) {}\n');save(runtime,'titles/futureperfect/src/recomp/gen/guest.c','PRIVATE');commit(runtime);
 const pin=git(runtime,'rev-parse','HEAD').trim();save(root,'tsfp.ps1',`$revision = '${pin}'\n`);git(root,'add','tsfp.ps1');
 const result=createRepositoryEvidence(root).retrieve({...read,namespace:'runtime',path:'src/kernel/gate.c'});assert.equal(result.status,'ok');assert.equal(result.source.revision,pin);
 assert.equal(createRepositoryEvidence(root).retrieve({...read,namespace:'runtime',path:'titles/futureperfect/src/recomp/gen/guest.c'}).reason,'file_not_allowlisted');
 save(runtime,'src/kernel/gate.c','changed');commit(runtime);assert.equal(createRepositoryEvidence(root).retrieve({...read,namespace:'runtime',path:'src/kernel/gate.c'}).reason,'runtime_revision_mismatch');
});

test('evidence protocol accepts only bounded typed requests',()=>{
 assert.equal(parseEvidenceReply('Actual concise advisory'),null);assert.equal(parseEvidenceReply(need([read])).requests.length,1);
 assert.throws(()=>parseEvidenceReply(need([{...read,path:'../auth.json'}])));
 assert.throws(()=>parseEvidenceReply(need([read,read,read,read])));
 assert.throws(()=>parseEvidenceReply('{"kind":"need_evidence",broken'));
});

test('actual local retrieval feeds the next advisory and counts every hosted request',async t=>{
 const root=fixture(t),abort=new AbortController();let attempts=0,calls=0;const inputs=[];
 const result=await runEvidenceAdvisory({state:{cwd:root},signal:abort.signal,guard:()=>attempts<6,attempt:()=>attempts++,success:()=>calls++,ask:async state=>{inputs.push(state);return {answer:inputs.length===1?need([read]):'The current function adds one. LOCAL should multiply by two and verify negative/zero inputs.'};}});
 assert.equal(attempts,2);assert.equal(calls,2);assert.equal(result.evidence.rounds,1);assert.match(inputs[1].evidenceExchange[0].result.text,/return n \+ 1/);assert.match(result.answer,/multiply by two/);
});
test('duplicates, exact-one budgets, invalid requests and total evidence rounds stop boundedly',async t=>{
 const root=fixture(t);let attempts=0,retrievals=0;
 const result=await runEvidenceAdvisory({state:{cwd:root},signal:new AbortController().signal,guard:()=>attempts<6,attempt:()=>attempts++,success:()=>{},broker:{retrieve:()=>{retrievals++;return {status:'ok',text:'bounded'};}},ask:async()=>({answer:need([read])})});
 assert.equal(attempts,3);assert.equal(retrievals,1);assert.equal(result.evidence.rounds,2);assert.ok(result.evidence.bytes<=evidenceLimits.totalBytes);assert.equal(result.evidence.limited,'budget');
 attempts=0;const one=await runEvidenceAdvisory({state:{cwd:root},signal:new AbortController().signal,guard:()=>attempts<1,attempt:()=>attempts++,success:()=>{},broker:{retrieve:()=>assert.fail('No follow-up budget')},ask:async()=>({answer:need([read])})});assert.equal(attempts,1);assert.equal(one.evidence.limited,'budget');
});
test('cancellation and a native restriction stop retrieval or follow-up',async t=>{
 const root=fixture(t),abort=new AbortController();let calls=0;
 await assert.rejects(runEvidenceAdvisory({state:{cwd:root},signal:abort.signal,guard:()=>true,attempt:()=>calls++,success:()=>{},broker:{retrieve:()=>assert.fail('Cancelled')},ask:async()=>{abort.abort();return {answer:need([read])};}}));assert.equal(calls,1);
 let permitted=true;await assert.rejects(runEvidenceAdvisory({state:{cwd:root},signal:new AbortController().signal,guard:()=>true,permitted:()=>permitted,attempt:()=>{},success:()=>{},broker:{retrieve:()=>assert.fail('Native denial')},ask:async()=>{permitted=false;return {answer:need([read])};}}),/restrictions changed/);
});
test('evidence byte exhaustion and file changes cannot silently yield a stale final plan',async t=>{
 const root=fixture(t);let calls=0;
 const bounded=await runEvidenceAdvisory({state:{cwd:root},signal:new AbortController().signal,guard:()=>calls<6,attempt:()=>calls++,success:()=>{},broker:{retrieve:()=>({status:'ok',text:'x'.repeat(10000)})},ask:async()=>({answer:need([read])})});
 assert.ok(bounded.evidence.bytes<=6500);assert.ok(calls<=3);
 calls=0;await assert.rejects(runEvidenceAdvisory({state:{cwd:root},signal:new AbortController().signal,guard:()=>true,attempt:()=>calls++,success:()=>{},ask:async()=>{if(calls===1)return {answer:need([read])};save(root,'src/counter.c','concurrent change\n');return {answer:'This stale plan must not be used.'};}}),/changed/);
});


test('nested responsibility patches retain effective order and exclude legacy',t=>{
 const root=fixture(t),hash=s=>createHash('sha256').update(s).digest('hex');
 const payload='--- a/src/example.c\n+++ b/src/example.c\n@@ -1 +1 @@\n-old_gate();\n+new_gate();\n';
 save(root,'tools/build/apply-patches.py','# fixture loader\n');
 save(root,'patches/graphics/rendering/fix.patch',payload);
 save(root,'patches/legacy/old.patch',payload);
 save(root,'docs/agent-context/patch-index.json',JSON.stringify({patches:[{name:'fix.patch',path:'patches/graphics/rendering/fix.patch',sha256:hash(payload)}],loaderSha256:hash('# fixture loader\n')}));commit(root);
 const result=createRepositoryEvidence(root).retrieve({kind:'patches',query:'new_gate'});
 assert.equal(result.hits.length,1);assert.equal(result.hits[0].order,1);
 assert.equal(result.hits[0].target,'src/example.c');
});
