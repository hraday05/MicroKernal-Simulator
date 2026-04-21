/* ============================================
   MicroKernel OS v5.0 — Backend-Connected Dashboard
   Terminal + Live Demo + API Layer
   ============================================ */

const API_BASE = window.location.origin;
const POLL_INTERVAL = 500;
const GANTT_COLORS = ['#42a5f5','#66bb6a','#ab47bc','#ffa726','#ef5350','#26c6da','#ffee58','#ec407a','#8d6e63','#78909c'];
let connected = false, autoTickInterval = null, lastConsoleLen = 0, lastGanttLen = 0, pollTimer = null, bootTime = Date.now(), termCmdCount = 0, demoRunning = false;

// =============== TERMINAL ===============
const terminalEl = document.getElementById('terminal-output');

function termWrite(text, cls = 'output') {
    const line = document.createElement('div');
    line.className = `term-line ${cls}`;
    line.textContent = text;
    terminalEl.appendChild(line);
    terminalEl.scrollTop = terminalEl.scrollHeight;
}

function termCmd(cmd) {
    termCmdCount++;
    document.getElementById('term-count').textContent = `${termCmdCount} commands`;
    termWrite(`>> ${cmd}`, 'cmd');
}

function termPhase(title) {
    termWrite(`\n═══ ${title} ═══`, 'phase-title');
}

// =============== API LAYER ===============
async function sendCommand(cmd) {
    termCmd(cmd);
    try {
        const res = await fetch(`${API_BASE}/api/command`, {
            method: 'POST', headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ cmd })
        });
        const data = await res.json();
        if (data.ok) termWrite(data.pid ? `OK (PID: ${data.pid})` : 'OK', 'output');
        else termWrite(`Error: ${data.error || 'failed'}`, 'output');
        return data;
    } catch (e) {
        termWrite(`[API Error] ${e.message}`, 'output');
        return { ok: false };
    }
}

async function fetchState() {
    try {
        const res = await fetch(`${API_BASE}/api/state`);
        const state = await res.json();
        if (!connected) { connected = true; updateConnectionStatus(true); log('success', '[Dashboard] Connected to C++ backend'); }
        renderState(state);
    } catch (e) {
        if (connected) { connected = false; updateConnectionStatus(false); log('error', '[Dashboard] Lost connection'); }
    }
}

// =============== RENDER STATE ===============
function renderState(s) {
    renderProcessNodes(s.scheduler?.processes || []);
    renderMemoryMap(s.memory);
    renderGanttChart(s.scheduler?.gantt || []);
    renderConsoleLog(s.console || []);
    updateStatusBar(s.scheduler, s.memory);
}

function renderProcessNodes(processes) {
    const c = document.getElementById('process-nodes-container');
    const living = processes.filter(p => p.state !== 'DEAD'), dead = processes.filter(p => p.state === 'DEAD').slice(-4);
    if (living.length + dead.length === 0) {
        if (!c.querySelector('.empty-state')) c.innerHTML = `<div class="empty-state"><span class="empty-icon">⚡</span><p>No processes yet</p></div>`;
        document.getElementById('proc-count').textContent = '0 processes'; return;
    }
    const e = c.querySelector('.empty-state'); if (e) e.remove();
    const display = [...living, ...dead];
    document.getElementById('proc-count').textContent = `${living.length} active`;
    display.forEach(proc => {
        let node = c.querySelector(`[data-pid="${proc.pid}"]`);
        if (!node) {
            node = document.createElement('div'); node.className = 'proc-node'; node.dataset.pid = proc.pid;
            node.innerHTML = `<div class="priority-badge">${proc.priority}</div><div class="pid">P${proc.pid}</div><div class="state-label">${proc.state}</div><div class="burst-label">${proc.remaining}/${proc.burst}</div>`;
            c.appendChild(node);
        } else { node.querySelector('.state-label').textContent = proc.state; node.querySelector('.burst-label').textContent = `${proc.remaining}/${proc.burst}`; }
        node.className = `proc-node ${proc.state.toLowerCase()}`;
    });
    c.querySelectorAll('.proc-node').forEach(el => { if (!display.find(p => p.pid === parseInt(el.dataset.pid))) el.remove(); });
}

function renderMemoryMap(mem) {
    if (!mem) return;
    const bar = document.getElementById('memory-bar'); bar.innerHTML = '';
    let used = 0;
    (mem.blocks || []).forEach(b => {
        const pct = b.size / (mem.totalPages||256) * 100, el = document.createElement('div');
        el.className = `mem-block ${b.free ? 'free' : 'used'}`; el.style.width = `${pct}%`;
        if (!b.free) { const ci = Math.abs((b.pid-100)%GANTT_COLORS.length); el.style.background = `linear-gradient(135deg,${GANTT_COLORS[ci]}66,${GANTT_COLORS[ci]}33)`; el.textContent = pct>5?`P${b.pid}`:''; used += b.size; }
        else el.textContent = pct>8?'FREE':'';
        el.title = `Frame ${b.start}|${b.size}pg|${b.free?'FREE':'PID '+b.pid}`; bar.appendChild(el);
    });
    document.getElementById('mem-usage').textContent = `${used} / ${mem.totalPages||256} pages`;
}

function renderGanttChart(gantt) {
    const tl = document.getElementById('gantt-timeline');
    if (!gantt||gantt.length===0) { if(lastGanttLen===0) tl.innerHTML='<div class="gantt-empty">Scheduling data appears as processes execute</div>'; return; }
    for (let i=lastGanttLen;i<gantt.length;i++) {
        if(lastGanttLen===0&&i===0) tl.innerHTML='';
        const e=gantt[i],d=e.end-e.start,w=Math.max(36,Math.min(150,d*12)),ci=Math.abs((e.pid-100)%GANTT_COLORS.length),col=GANTT_COLORS[ci];
        const bl=document.createElement('div'); bl.className='gantt-block'; bl.style.width=`${w}px`; bl.style.background=`linear-gradient(135deg,${col}cc,${col}66)`; bl.style.border=`1px solid ${col}`;
        bl.innerHTML=`P${e.pid}<span style="display:block;font-size:0.5rem;opacity:0.7">${d}u</span>`; bl.dataset.time=`t=${e.start}`; tl.appendChild(bl);
    }
    lastGanttLen=gantt.length; tl.scrollLeft=tl.scrollWidth;
}

function renderConsoleLog(entries) {
    if(!entries||entries.length===0) return;
    if(entries.length>lastConsoleLen) {
        const el=document.getElementById('console-output');
        for(let i=lastConsoleLen;i<entries.length;i++) { const l=document.createElement('div'); l.className=`console-line ${entries[i].type||'info'}`; l.textContent=entries[i].msg; el.appendChild(l); }
        lastConsoleLen=entries.length; el.scrollTop=el.scrollHeight;
        document.getElementById('log-count').textContent=`${el.children.length} events`;
    }
}

function updateStatusBar(sched,mem) {
    if(!sched) return;
    const am={'Round Robin':'RR','Priority':'PRI','SJF':'SJF'};
    document.getElementById('sched-chip').textContent=am[sched.algorithm]||sched.algorithm;
    document.getElementById('mem-chip').textContent=mem?.algorithm||'First Fit';
    document.getElementById('time-chip').textContent=`t=${sched.cpuTime||0}`;
    const cc=document.getElementById('cpu-chip');
    if(sched.currentPid>=0){cc.textContent=`CPU: P${sched.currentPid}`;cc.className='status-chip running';}
    else{cc.textContent='CPU: IDLE';cc.className='status-chip';}
    document.getElementById('gantt-algo').textContent=`${sched.algorithm} (Q=${sched.quantum||5})`;
}

function updateConnectionStatus(ok) {
    const d=document.getElementById('connection-status');
    if(d){d.className=`connection-dot ${ok?'connected':'disconnected'}`;d.title=ok?'Connected':'Disconnected';}
}

function log(type,msg) {
    const el=document.getElementById('console-output'),l=document.createElement('div'); l.className=`console-line ${type}`;
    const ms=Date.now()-bootTime,s=Math.floor(ms/1000)%60,m=Math.floor(ms/60000);
    l.textContent=`[${String(m).padStart(2,'0')}:${String(s).padStart(2,'0')}.${String(ms%1000).padStart(3,'0')}] ${msg}`;
    el.appendChild(l); el.scrollTop=el.scrollHeight;
}

// =============== MODAL ===============
const modalOverlay = document.getElementById('modal-overlay');
function showModal(title, fields, callback) {
    document.getElementById('modal-title').textContent = title;
    const c = document.getElementById('modal-fields'); c.innerHTML = '';
    fields.forEach(f => {
        const d = document.createElement('div'); d.className = 'modal-field';
        if (f.type==='select') d.innerHTML=`<label>${f.label}</label><select id="modal-${f.id}">${f.options.map(o=>`<option value="${o.value}">${o.label}</option>`).join('')}</select>`;
        else d.innerHTML=`<label>${f.label}</label><input type="${f.type||'text'}" id="modal-${f.id}" value="${f.default||''}" placeholder="${f.placeholder||''}">`;
        c.appendChild(d);
    });
    modalOverlay.classList.add('show');
    document.getElementById('modal-confirm').onclick=()=>{const v={};fields.forEach(f=>{v[f.id]=document.getElementById(`modal-${f.id}`).value;});modalOverlay.classList.remove('show');callback(v);};
    document.getElementById('modal-cancel').onclick=()=>modalOverlay.classList.remove('show');
    setTimeout(()=>{const inp=c.querySelector('input,select');if(inp)inp.focus();},100);
}

// =============== COMMANDS ===============
const capOpts = [{value:'file',label:'CAP_FILE'},{value:'mem',label:'CAP_MEM'},{value:'ipc',label:'CAP_IPC'},{value:'proc',label:'CAP_PROC'},{value:'sched',label:'CAP_SCHED'},{value:'kill',label:'CAP_KILL'}];
const commands = {
    create_process:()=>showModal('Create Process',[{id:'burst',label:'Burst Time',type:'number',default:'30'},{id:'priority',label:'Priority (1-10)',type:'number',default:'5'}],v=>sendCommand(`create_process ${v.burst||30} ${Math.max(1,Math.min(10,parseInt(v.priority)||5))}`)),
    list_process:()=>sendCommand('list_process'), ps:()=>log('kernel','Process snapshot in visualization panel'),
    kill:()=>showModal('Kill',[{id:'pid',label:'PID',type:'number',placeholder:'100'}],v=>sendCommand(`kill ${v.pid}`)),
    suspend:()=>showModal('Suspend',[{id:'pid',label:'PID',type:'number',placeholder:'100'}],v=>sendCommand(`suspend ${v.pid}`)),
    resume:()=>showModal('Resume',[{id:'pid',label:'PID',type:'number',placeholder:'100'}],v=>sendCommand(`resume ${v.pid}`)),
    set_rr:()=>{sendCommand('set_scheduler rr');uBtn('#sched-cmds','set_rr');}, set_priority:()=>{sendCommand('set_scheduler priority');uBtn('#sched-cmds','set_priority');}, set_sjf:()=>{sendCommand('set_scheduler sjf');uBtn('#sched-cmds','set_sjf');},
    tick:()=>sendCommand('tick'), gantt:()=>log('info','Gantt chart rendered above'), schedstat:()=>log('info','Stats in header bar'),
    alloc:()=>showModal('Allocate Memory',[{id:'pid',label:'PID',type:'number',placeholder:'100'},{id:'bytes',label:'Bytes',type:'number',default:'8192'}],v=>sendCommand(`alloc ${v.pid} ${v.bytes}`)),
    free_mem:()=>showModal('Free Memory',[{id:'pid',label:'PID',type:'number',placeholder:'100'}],v=>sendCommand(`free ${v.pid}`)),
    set_first:()=>{sendCommand('set_memory first');uBtn('#mem-cmds','set_first');}, set_best:()=>{sendCommand('set_memory best');uBtn('#mem-cmds','set_best');}, set_worst:()=>{sendCommand('set_memory worst');uBtn('#mem-cmds','set_worst');},
    memmap:()=>log('info','Memory map rendered above'),
    create_file:()=>showModal('Create File',[{id:'name',label:'Filename',placeholder:'myfile.txt'}],v=>sendCommand(`create_file ${v.name}`)),
    read_file:()=>showModal('Read File',[{id:'name',label:'Filename',placeholder:'myfile.txt'}],v=>sendCommand(`read_file ${v.name}`)),
    write_file:()=>showModal('Write File',[{id:'name',label:'Filename',placeholder:'myfile.txt'},{id:'data',label:'Content',placeholder:'Hello'}],v=>sendCommand(`write_file ${v.name} ${v.data}`)),
    delete_file:()=>showModal('Delete File',[{id:'name',label:'Filename',placeholder:'myfile.txt'}],v=>sendCommand(`delete_file ${v.name}`)),
    chmod_file:()=>showModal('Chmod',[{id:'name',label:'Filename',placeholder:'myfile.txt'},{id:'perm',label:'Permission',type:'select',options:[{value:'both',label:'Read+Write'},{value:'read',label:'Read Only'},{value:'write',label:'Write Only'},{value:'none',label:'None'}]}],v=>sendCommand(`chmod ${v.name} ${v.perm}`)),
    ls:()=>log('info','Files shown in state'), grant:()=>showModal('Grant Cap',[{id:'pid',label:'PID',type:'number',placeholder:'100'},{id:'cap',label:'Capability',type:'select',options:capOpts}],v=>sendCommand(`grant ${v.pid} ${v.cap}`)),
    revoke:()=>showModal('Revoke Cap',[{id:'pid',label:'PID',type:'number',placeholder:'100'},{id:'cap',label:'Capability',type:'select',options:capOpts}],v=>sendCommand(`revoke ${v.pid} ${v.cap}`)),
    capabilities:()=>log('info','Capabilities in state data'),
    hack_file:()=>showModal('🏴‍☠️ Hack',[{id:'name',label:'Target File',placeholder:'secret.txt'}],v=>sendCommand(`hack_file ${v.name}`)),
    attack_demo:()=>sendCommand('attack_demo'),
    lock:()=>showModal('Lock Resource',[{id:'pid',label:'PID',type:'number',placeholder:'100'},{id:'res',label:'Resource',placeholder:'fileA'}],v=>sendCommand(`lock ${v.pid} ${v.res}`)),
    unlock:()=>showModal('Unlock Resource',[{id:'pid',label:'PID',type:'number',placeholder:'100'},{id:'res',label:'Resource',placeholder:'fileA'}],v=>sendCommand(`unlock ${v.pid} ${v.res}`)),
    deadlock:()=>sendCommand('deadlock'), resources:()=>log('info','Resources in state'),
    ipc_create:()=>showModal('Create Channel',[{id:'name',label:'Channel',placeholder:'data_pipe'},{id:'pid',label:'Owner PID',type:'number',placeholder:'100'}],v=>sendCommand(`ipc_create ${v.name} ${v.pid}`)),
    ipc_send:()=>showModal('Send',[{id:'name',label:'Channel',placeholder:'data_pipe'},{id:'msg',label:'Message',placeholder:'Hello'}],v=>sendCommand(`ipc_send ${v.name} ${v.msg}`)),
    ipc_recv:()=>showModal('Receive',[{id:'name',label:'Channel',placeholder:'data_pipe'}],v=>sendCommand(`ipc_recv ${v.name}`)),
    ipc_list:()=>log('info','Channels in state'), syslog:()=>log('info','Log in state data'),
    kill_service:()=>sendCommand('kill_service'),
    clear:()=>{document.getElementById('console-output').innerHTML='';document.getElementById('terminal-output').innerHTML='';lastConsoleLen=0;termCmdCount=0;},
};
function uBtn(g,a){document.querySelectorAll(`${g} .cmd-btn`).forEach(b=>b.classList.remove('active'));const b=document.querySelector(`[data-cmd="${a}"]`);if(b)b.classList.add('active');}

// =============== LIVE DEMO ===============
const delay = ms => new Promise(r => setTimeout(r, ms));

async function runLiveDemo() {
    if (demoRunning) return;
    demoRunning = true;
    const btn = document.getElementById('btn-live-demo');
    const status = document.getElementById('demo-status');
    btn.classList.add('running'); btn.textContent = '⏳ Demo Running...';

    // Clear
    document.getElementById('console-output').innerHTML = '';
    document.getElementById('terminal-output').innerHTML = '';
    lastConsoleLen = 0; lastGanttLen = 0; termCmdCount = 0;
    termWrite('MicroKernel OS v5.0 — LIVE DEMO', 'prompt');

    // ===== PHASE 1: Process Creation =====
    termPhase('PHASE 1: Process Creation');
    status.textContent = 'Phase 1/9: Creating processes...';
    await sendCommand('create_process 50 2'); await delay(800);
    await sendCommand('create_process 30 5'); await delay(800);
    await sendCommand('create_process 80 8'); await delay(1000);

    // ===== PHASE 2: Memory Allocation =====
    termPhase('PHASE 2: Memory Allocation');
    status.textContent = 'Phase 2/9: Allocating memory...';
    await sendCommand('alloc 100 16384'); await delay(600);
    await sendCommand('alloc 101 8192'); await delay(600);
    await sendCommand('set_memory best'); await delay(400);
    await sendCommand('alloc 102 32768'); await delay(1000);

    // ===== PHASE 3: Scheduling =====
    termPhase('PHASE 3: CPU Scheduling (RR → Priority → SJF)');
    status.textContent = 'Phase 3/9: Running scheduler...';
    await sendCommand('tick'); await delay(500);
    await sendCommand('tick'); await delay(500);
    await sendCommand('tick'); await delay(500);
    await sendCommand('set_scheduler priority'); await delay(400);
    await sendCommand('tick'); await delay(500);
    await sendCommand('tick'); await delay(500);
    await sendCommand('set_scheduler sjf'); await delay(400);
    await sendCommand('tick'); await delay(500);
    await sendCommand('set_scheduler rr'); await delay(800);

    // ===== PHASE 4: File System =====
    termPhase('PHASE 4: Persistent File System');
    status.textContent = 'Phase 4/9: File operations...';
    await sendCommand('create_file server.log'); await delay(600);
    await sendCommand('write_file server.log HTTP_200_OK_request_served'); await delay(600);
    await sendCommand('create_file config.ini'); await delay(600);
    await sendCommand('write_file config.ini port=8080_workers=4'); await delay(600);
    await sendCommand('read_file server.log'); await delay(600);
    await sendCommand('chmod server.log read'); await delay(800);

    // ===== PHASE 5: IPC Channels =====
    termPhase('PHASE 5: IPC Message Passing');
    status.textContent = 'Phase 5/9: IPC channels...';
    await sendCommand('ipc_create data_pipe 100'); await delay(600);
    await sendCommand('ipc_send data_pipe result=42_status=complete'); await delay(600);
    await sendCommand('ipc_send data_pipe heartbeat_alive'); await delay(600);
    await sendCommand('ipc_recv data_pipe'); await delay(1000);

    // ===== PHASE 6: Security & Sandbox =====
    termPhase('PHASE 6: Security — Attack Demo');
    status.textContent = 'Phase 6/9: Security sandbox...';
    await sendCommand('attack_demo'); await delay(2000);

    // ===== PHASE 7: Deadlock Detection =====
    termPhase('PHASE 7: Deadlock Detection');
    status.textContent = 'Phase 7/9: Deadlock detection...';
    await sendCommand('lock 100 resourceA'); await delay(600);
    await sendCommand('lock 101 resourceB'); await delay(600);
    await sendCommand('lock 100 resourceB'); await delay(600);
    await sendCommand('lock 101 resourceA'); await delay(600);
    await sendCommand('deadlock'); await delay(1000);

    // ===== PHASE 8: Service Recovery =====
    termPhase('PHASE 8: Service Crash Recovery');
    status.textContent = 'Phase 8/9: Crash & recovery...';
    await sendCommand('kill_service'); await delay(1500);

    // ===== PHASE 9: Process Signals =====
    termPhase('PHASE 9: Process Signals');
    status.textContent = 'Phase 9/9: Suspend/Resume/Kill...';
    await sendCommand('suspend 101'); await delay(800);
    await sendCommand('tick'); await delay(600);
    await sendCommand('resume 101'); await delay(800);
    await sendCommand('kill 102'); await delay(1000);

    // ===== DONE =====
    termPhase('DEMO COMPLETE ✅');
    termWrite('All 9 features demonstrated successfully!', 'prompt');
    termWrite('Processes • Scheduling • Memory • Files • IPC • Security • Deadlock • Recovery • Signals', 'system');

    status.textContent = 'Demo complete! ✅';
    btn.classList.remove('running'); btn.textContent = '🎬 Live Demo';
    demoRunning = false;
}

// =============== EVENT WIRING ===============
document.querySelectorAll('.cmd-btn').forEach(btn => { btn.addEventListener('click', () => { const cmd = btn.dataset.cmd; if (commands[cmd]) commands[cmd](); }); });

document.getElementById('btn-auto-tick').addEventListener('click', function () {
    if (autoTickInterval) { clearInterval(autoTickInterval); autoTickInterval = null; this.textContent = '▶ Auto'; this.classList.remove('active'); log('info', '[Scheduler] Auto-tick stopped'); }
    else { autoTickInterval = setInterval(() => sendCommand('tick'), 1000); this.textContent = '⏸ Stop'; this.classList.add('active'); log('info', '[Scheduler] Auto-tick started'); }
});

document.getElementById('btn-live-demo').addEventListener('click', runLiveDemo);

document.querySelectorAll('.cmd-group-title').forEach(t => { t.addEventListener('click', () => { const tg = document.getElementById(t.dataset.toggle); if(tg) tg.style.display = tg.style.display==='none'?'flex':'none'; }); });

// Terminal input
document.getElementById('terminal-input').addEventListener('keydown', e => {
    if (e.key === 'Enter') {
        const input = e.target;
        const cmd = input.value.trim();
        if (cmd) { sendCommand(cmd); input.value = ''; }
    }
});

modalOverlay.addEventListener('click', e => { if (e.target === modalOverlay) modalOverlay.classList.remove('show'); });
document.addEventListener('keydown', e => { if (e.key === 'Escape') modalOverlay.classList.remove('show'); });

// =============== START ===============
log('boot', '[Dashboard] Connecting to C++ backend...');
pollTimer = setInterval(fetchState, POLL_INTERVAL);
fetchState();
