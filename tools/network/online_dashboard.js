'use strict';
const $=id=>document.getElementById(id);
let latest=null, selected=null, lastOK=0;
const number=n=>Number(n||0).toLocaleString(undefined,{maximumFractionDigits:0});
const decimal=n=>Number(n||0).toLocaleString(undefined,{maximumFractionDigits:1});
const bytes=n=>n==null?'Unavailable':n<1048576?`${decimal(n/1024)} KiB`:`${decimal(n/1048576)} MiB`;
const seconds=n=>n==null?'Not reported':`${decimal(n)}s`;
function element(tag,text,className){const node=document.createElement(tag);if(text!=null)node.textContent=text;if(className)node.className=className;return node;}
function badge(text,style){return element('span',text,`badge ${style||''}`);}
function uptime(n){const days=Math.floor(n/86400),hours=Math.floor(n%86400/3600),minutes=Math.floor(n%3600/60);return `${days?`${days}d `:''}${hours}h ${minutes}m`;}
function drawChart(){
 if(!latest)return;
 const metric=$('chart-metric').value,scale=['tx','rx'].includes(metric)?1024:1;
 const samples=latest.samples,maximum=Math.max(1,...samples.map(s=>s[metric]/scale))*1.15;
 const last=samples.length?samples[samples.length-1].at:Date.now()/1000;
 const points=samples.map(s=>[Math.max(0,800*(1+(s.at-last)/180)),179-176*s[metric]/scale/maximum]);
 const path=points.map((p,i)=>`${i?'L':'M'}${p[0].toFixed(1)},${p[1].toFixed(1)}`).join(' ');
 $('chart-line').setAttribute('d',path);
 $('chart-area').setAttribute('d',points.length?`${path} L800,180 L${points[0][0].toFixed(1)},180 Z`:'');
 $('chart-max').textContent=decimal(maximum);
 $('chart-label').textContent=metric==='packets'?'Forwarded packets':metric==='tx'?'Outgoing relay traffic':'Incoming relay traffic';
 $('chart-current').textContent=`${decimal((samples.length?samples[samples.length-1][metric]:0)/scale)} ${metric==='packets'?'pkt/s':'KiB/s'}`;
}
function drawRooms(){
 if(!latest)return;
 const filter=$('lobby-filter').value;
 const rooms=latest.rooms.filter(r=>filter==='all'||(filter==='active'?['active','preparing'].includes(r.phase):r.phase==='lobby'));
 const rows=$('lobby-rows'),focused=rows.contains(document.activeElement)?document.activeElement.dataset.room:null;rows.replaceChildren();
 if(!rooms.some(r=>r.id===selected))selected=rooms.length?rooms[0].id:null;
 $('lobby-empty').classList.toggle('hidden',rooms.length>0);
 $('lobby-empty').querySelector('h3').textContent=latest.rooms.length?'No matching lobbies':'No active lobbies';
 $('lobby-empty').querySelector('p').textContent=latest.rooms.length?'Choose another session filter to see your other lobbies.':'Start an Online lobby in the game. It will appear here automatically.';
 for(const room of rooms){
  const row=element('tr',null,room.id===selected?'selected':'');
  const name=element('td'),button=element('button',room.name,'lobby-button');button.type='button';button.dataset.room=room.id;button.setAttribute('aria-expanded',String(room.id===selected));button.addEventListener('click',()=>{selected=room.id;drawRooms();});name.append(button,element('small',`${room.protocol} · ${room.mode}`));row.append(name);
  const phase=element('td');phase.append(badge({active:'Playing',preparing:'Changing host',lobby:'Waiting',legacy:'Legacy session',closed:'Closed'}[room.phase]||room.phase,room.phase));row.append(phase);
  row.append(element('td',`${room.players} / ${room.capacity}`));row.append(element('td',room.host?`Player ${room.host}`:room.phase==='preparing'?'Selecting…':'—'));
  const backup=element('td');if(room.backup){backup.append(element('strong',`#${room.backup.sequence} · ${seconds(room.backup.age)} ago`),element('small',`${room.backup.acknowledgements} engine ACK${room.backup.acknowledgements===1?'':'s'}${room.backup.bots!=null?` · ${room.backup.bots} bots`:''}`));}else backup.textContent=room.migration_supported?'Waiting for game snapshot':'Not available';row.append(backup);rows.append(row);
 }
 if(focused){const replacement=[...rows.querySelectorAll('button')].find(button=>button.dataset.room===focused);if(replacement)replacement.focus({preventScroll:true});}
 const room=rooms.find(r=>r.id===selected);$('room-detail').classList.toggle('hidden',!room);if(!room)return;
 $('detail-name').textContent=room.name;$('detail-epoch').textContent=room.epoch?`Epoch ${room.epoch}`:'';
 $('legacy-note').classList.toggle('hidden',room.migration_supported);
 const cards=$('peer-cards');cards.replaceChildren();
 for(const peer of room.peers){
  const card=element('article',null,'peer'),title=element('div',null,'peer-title');title.append(element('span',`Player ${peer.id}`),badge(peer.host?'HOST':peer.candidate?'NEXT HOST':peer.active===true?'PLAYING':peer.active===false?'WAITING':'GUEST',peer.host?'active':peer.candidate?'preparing':'lobby'));card.append(title);
  const data=element('dl');
  const values=[['Relay',peer.registered?`${peer.route} · registered`:`${peer.route} · ${peer.relay_age==null?'waiting':'stale'}`],['Native player ID',peer.native_id==null?'Not reported':peer.native_id],['Engine update',peer.progress_age==null?'Not reported':`${seconds(peer.progress_age)} ago`],['Migration lease',peer.lease_remaining==null?'Not active':`${seconds(peer.lease_remaining)} left`],['Backup',room.backup?(peer.backup_ack?'Acknowledged':'Awaiting ACK'):'No snapshot yet']];
  for(const [label,value] of values){const line=element('div');line.append(element('dt',label),element('dd',value,label==='Engine update'&&peer.active&&peer.progress_age>3?'warning':''));data.append(line);}card.append(data);cards.append(card);
 }
}
function drawEvents(){
 const target=$('events'),scroll=target.scrollTop;target.replaceChildren();$('event-count').textContent=latest.events.length;
 if(!latest.events.length){target.append(element('div','No recent events. Lobby and connection activity will appear here.','empty small'));return;}
 for(const event of latest.events){const row=element('div',null,`event ${event.level}`),message=element('div',null,'event-message'),timestamp=element('time',new Date(event.at*1000).toLocaleTimeString(undefined,{hour12:false}));timestamp.dateTime=new Date(event.at*1000).toISOString();message.append(element('span',event.message),element('small',event.kind));row.append(element('span',event.level==='warning'?'!':event.kind==='host'?'↔':'·','event-icon'),message,timestamp);target.append(row);}
 target.scrollTop=scroll;
}
function draw(){
 const d=latest,playing=d.rooms.filter(r=>r.phase==='active').length,migrating=d.rooms.filter(r=>r.phase==='preparing').length;
 $('machine-name').textContent=d.hostname;$('room-count').textContent=d.rooms.length;$('lobby-heading-count').textContent=d.rooms.length;
 $('match-count').textContent=`${playing} playing · ${migrating} changing host`;$('player-count').textContent=d.players;
 $('registered-count').textContent=`${d.rooms.reduce((sum,r)=>sum+r.peers.filter(p=>p.registered).length,0)} relay registrations current`;
 $('traffic-rate').textContent=decimal(d.rates.sent_bytes/1024);$('traffic-total').textContent=`${bytes(d.counters.sent_bytes)} sent since service start`;
 const failures=d.http.failures,alerts=d.http.errors+(failures.timeout||0)+(failures.tls||0)+(failures.internal||0);
 $('alert-count').textContent=number(alerts);$('alert-detail').textContent=`${number(d.http.errors)} HTTP errors · ${number(failures.timeout)} timeouts`;
 $('packets-total').textContent=number(d.counters.forwarded);$('dropped-total').textContent=number(d.counters.dropped);
 $('platform').textContent=`${d.platform} · ${d.hostname}`;$('uptime').textContent=uptime(d.uptime);$('cpu').textContent=`${decimal(d.cpu_percent)}%`;$('memory').textContent=bytes(d.peak_memory);
 $('workers').textContent=`${d.http.workers} / ${d.http.worker_limit}`;$('latency').textContent=`${decimal(d.http.p95_ms)} ms`;$('ports').textContent=`${d.directory_port||'—'} / ${d.relay_port||'Off'}`;
 $('footer-update').textContent=`${number(d.http.requests)} directory requests · ${d.relay_enabled?'Relay enabled':'Relay disabled'}`;
 drawChart();drawRooms();drawEvents();tunnelCommand();
}
function tunnelCommand(){const host=$('ssh-host').value.trim();const port=location.port||41822,remote=latest?.dashboard_port||41822;const command=host&&/^[A-Za-z0-9_.@:-]+$/.test(host)&&!host.startsWith('-')?`ssh -N -o ExitOnForwardFailure=yes -L 127.0.0.1:${port}:127.0.0.1:${remote} ${host}`:'';$('tunnel-command').textContent=command||'Enter the mini’s SSH name or address above.';return command;}
$('ssh-host').addEventListener('input',tunnelCommand);
$('copy-tunnel').addEventListener('click',async()=>{const command=tunnelCommand();if(!command){$('copy-status').textContent='Enter a valid SSH name or address first.';return;}try{await navigator.clipboard.writeText(command);$('copy-status').textContent='Command copied. Run it in a terminal on the computer you want to view from.';}catch{$('copy-status').textContent='Select and copy the command above.';}});
$('chart-metric').addEventListener('change',drawChart);$('lobby-filter').addEventListener('change',drawRooms);
async function refresh(){
 const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),2500);
 try{const response=await fetch('/api/status',{signal:controller.signal,cache:'no-store'});if(!response.ok)throw Error('Dashboard unavailable');latest=await response.json();lastOK=Date.now();draw();const stale=latest.sample_age>4;$('connection').className=stale?'badge waiting':'badge';$('connection').replaceChildren(element('i'),document.createTextNode(stale?'Telemetry delayed':'Service online'));$('offline').classList.toggle('hidden',!stale);if(stale)$('offline').textContent='The service is reachable, but telemetry sampling is delayed. Displayed values are from the last sample.';$('updated').textContent=`Updated ${new Date(latest.at*1000).toLocaleTimeString(undefined,{hour12:false})}`;}
 catch{$('connection').className='badge offline';$('connection').replaceChildren(element('i'),document.createTextNode('Disconnected'));$('offline').classList.remove('hidden');$('offline').textContent='Connection to the dashboard was lost. Displayed values are from the last successful update. Reconnecting…';$('updated').textContent=lastOK?`Last update ${Math.floor((Date.now()-lastOK)/1000)}s ago`:'Waiting for the mini';}
 finally{clearTimeout(timer);setTimeout(refresh,1000);}
}
refresh();
