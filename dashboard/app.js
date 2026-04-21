/* ============================================
   MicroKernel OS Simulator v5.0 — Dashboard JS
   Full simulation engine + animated UI
   ============================================ */

// =============== SIMULATION ENGINE ===============

const TOTAL_PAGES = 256;
const PAGE_SIZE = 4096;
const GANTT_COLORS = ['#42a5f5','#66bb6a','#ab47bc','#ffa726','#ef5350','#26c6da','#ffee58','#ec407a','#8d6e63','#78909c'];
let nextPID = 100;
let cpuTime = 0;
let bootTime = Date.now();
let autoTickInterval = null;
let contextSwitches = 0;
let completions = 0;

// ---- Scheduler State ----
let algorithm = 'RR';
let timeQuantum = 5;
let readyList = [];
let blockedList = [];
let currentRunning = null;
let ganttLog = [];

// ---- Process List ----
let allProcesses = [];      // all ever created (for node display)
let processMap = {};         // pid -> process

// ---- Memory State ----
let memAlgorithm = 'FIRST';
let memBlocks = [{ start: 0, size: TOTAL_PAGES, free: true, pid: -1 }];

// ---- File System ----
let files = {};

// ---- Security ----
let capabilities = { 1: new Set(['CAP_FILE', 'CAP_MEM']) };

// ---- Resources & Deadlock ----
let resources = {};

// ---- IPC Channels ----
let channels = {};

// ---- Semaphores ----
let semaphores = {};

// ---- System Log ----
let sysLog = [];

// =============== PROCESS ===============

function createProcess(burst = 30, priority = 5) {
    const pid = nextPID++;
    const proc = {
        pid, name: `Process_${pid}`, burst, remaining: burst,
        priority, state: 'READY', colorIdx: (pid - 100) % GANTT_COLORS.length
    };
    readyList.push(proc);
    allProcesses.push(proc);
    processMap[pid] = proc;
    capabilities[pid] = new Set(['CAP_FILE', 'CAP_MEM']);
    log('success', `[ProcessServer] Created PID ${pid} (Burst: ${burst}, Priority: ${priority})`);
    sysLog.push({ t: elapsed(), msg: `Created PID ${pid} burst=${burst} pri=${priority}` });
    updateUI();
    return pid;
}

function killProcess(pid) {
    const proc = processMap[pid];
    if (!proc || proc.state === 'DEAD') { log('error', `[Scheduler] PID ${pid} not found`); return; }

    // Remove from wherever it is
    if (currentRunning && currentRunning.pid === pid) { currentRunning = null; }
    readyList = readyList.filter(p => p.pid !== pid);
    blockedList = blockedList.filter(p => p.pid !== pid);
    proc.state = 'DEAD';
    freeAllMemory(pid);
    delete capabilities[pid];
    completions++;
    log('warning', `[Scheduler] KILLED PID ${pid}`);
    sysLog.push({ t: elapsed(), msg: `KILLED PID ${pid}` });
    updateUI();
}

function suspendProcess(pid) {
    const proc = processMap[pid];
    if (!proc) { log('error', `PID ${pid} not found`); return; }

    if (currentRunning && currentRunning.pid === pid) {
        currentRunning.state = 'BLOCKED';
        blockedList.push(currentRunning);
        currentRunning = null;
    } else {
        const idx = readyList.findIndex(p => p.pid === pid);
        if (idx >= 0) {
            readyList[idx].state = 'BLOCKED';
            blockedList.push(readyList[idx]);
            readyList.splice(idx, 1);
        } else { log('error', `PID ${pid} not in scheduler`); return; }
    }
    log('warning', `[Scheduler] SUSPENDED PID ${pid} (→ BLOCKED)`);
    sysLog.push({ t: elapsed(), msg: `SUSPENDED PID ${pid}` });
    updateUI();
}

function resumeProcess(pid) {
    const idx = blockedList.findIndex(p => p.pid === pid);
    if (idx < 0) { log('error', `PID ${pid} not in blocked list`); return; }
    blockedList[idx].state = 'READY';
    readyList.push(blockedList[idx]);
    blockedList.splice(idx, 1);
    log('success', `[Scheduler] RESUMED PID ${pid} (→ READY)`);
    sysLog.push({ t: elapsed(), msg: `RESUMED PID ${pid}` });
    updateUI();
}

// =============== SCHEDULER ===============

function pickNextIndex() {
    if (readyList.length === 0) return -1;
    if (algorithm === 'RR') return 0;
    if (algorithm === 'PRIORITY') {
        let best = 0;
        for (let i = 1; i < readyList.length; i++) {
            if (readyList[i].priority < readyList[best].priority) best = i;
        }
        return best;
    }
    // SJF
    let best = 0;
    for (let i = 1; i < readyList.length; i++) {
        if (readyList[i].remaining < readyList[best].remaining) best = i;
    }
    return best;
}

let lastEffectiveQ = 5;  // track for UI display

function schedulerTick() {
    if (readyList.length === 0 && !currentRunning) return;

    // Pick if none running
    if (!currentRunning && readyList.length > 0) {
        const idx = pickNextIndex();
        currentRunning = readyList[idx];
        readyList.splice(idx, 1);
        currentRunning.state = 'RUNNING';
        contextSwitches++;
    }
    if (!currentRunning) return;

    // ===== ADAPTIVE TIME QUANTUM (only for RR) =====
    const numActive = readyList.length + 1;
    let effQ = timeQuantum;

    if (algorithm === 'RR') {
        if (numActive === 1) {
            // Alone — run up to 10x quantum
            effQ = timeQuantum * 10;
        } else if (numActive === 2) {
            // Only 2 processes — scale based on remaining burst
            const avgRemaining = (currentRunning.remaining + (readyList[0]?.remaining || 0)) / 2;
            if (avgRemaining > 100) {
                effQ = timeQuantum * 3;   // 3x — both have large burst, reduce switches
            } else if (avgRemaining > 40) {
                effQ = timeQuantum * 2;   // 2x — moderate burst
            } else {
                effQ = timeQuantum;       // 1x base — low burst, finishing soon anyway
            }
        } else if (numActive <= 4) {
            effQ = timeQuantum * 2;       // 2x for 3-4 processes
        }
        // else: 5+ processes → base quantum for fairness

        // Show quantum change in console
        if (effQ !== lastEffectiveQ) {
            const reason = numActive === 2
                ? `2 processes, avg remaining=${Math.round((currentRunning.remaining + (readyList[0]?.remaining || 0))/2)}`
                : `${numActive} active processes`;
            log('scheduler', `[Adaptive] Quantum: ${lastEffectiveQ} → ${effQ} (${reason})`);
            lastEffectiveQ = effQ;
        }
    } else {
        // Priority & SJF: non-preemptive, run to completion
        effQ = currentRunning.remaining;
    }

    const runTime = Math.min(effQ, currentRunning.remaining);

    // Gantt entry — includes quantum info
    ganttLog.push({
        pid: currentRunning.pid, start: cpuTime, end: cpuTime + runTime,
        color: GANTT_COLORS[currentRunning.colorIdx],
        quantum: effQ, algo: algorithm
    });

    cpuTime += runTime;
    currentRunning.remaining -= runTime;

    // Log each tick visibly
    log('info', `  P${currentRunning.pid} ran for ${runTime} units (remaining: ${currentRunning.remaining}, Q=${effQ})`);

    if (currentRunning.remaining <= 0) {
        // Process complete
        currentRunning.state = 'DEAD';
        completions++;
        log('scheduler', `[${getAlgoName()}] PID ${currentRunning.pid} completed (CPU: ${currentRunning.burst})`);
        sysLog.push({ t: elapsed(), msg: `PID ${currentRunning.pid} completed` });
        freeAllMemory(currentRunning.pid);
        delete capabilities[currentRunning.pid];
        currentRunning = null;

        if (readyList.length > 0) {
            const idx = pickNextIndex();
            currentRunning = readyList[idx];
            readyList.splice(idx, 1);
            currentRunning.state = 'RUNNING';
            contextSwitches++;
            log('kernel', `[Context Switch] → P${currentRunning.pid}`);
        }
    } else {
        if (readyList.length === 0) {
            // skip preemption — no one else waiting
        } else {
            currentRunning.state = 'READY';
            readyList.push(currentRunning);
            const idx = pickNextIndex();
            const prevPid = currentRunning.pid;
            currentRunning = readyList[idx];
            readyList.splice(idx, 1);
            currentRunning.state = 'RUNNING';
            contextSwitches++;
            log('kernel', `[Context Switch] P${prevPid} → P${currentRunning.pid}`);
        }
    }
    updateUI();
}

function getAlgoName() {
    return { RR: 'Round Robin', PRIORITY: 'Priority', SJF: 'SJF' }[algorithm] || algorithm;
}

// =============== MEMORY ===============

function findFirstFit(pages) { return memBlocks.findIndex(b => b.free && b.size >= pages); }

function findBestFit(pages) {
    let best = -1, bestSize = Infinity;
    memBlocks.forEach((b, i) => { if (b.free && b.size >= pages && b.size < bestSize) { best = i; bestSize = b.size; }});
    return best;
}

function findWorstFit(pages) {
    let worst = -1, worstSize = -1;
    memBlocks.forEach((b, i) => { if (b.free && b.size >= pages && b.size > worstSize) { worst = i; worstSize = b.size; }});
    return worst;
}

function allocMemory(pid, bytes) {
    const pages = Math.ceil(bytes / PAGE_SIZE);
    let idx;
    if (memAlgorithm === 'FIRST') idx = findFirstFit(pages);
    else if (memAlgorithm === 'BEST') idx = findBestFit(pages);
    else idx = findWorstFit(pages);

    if (idx === -1) { log('error', `[MemoryService] OOM! No block for ${pages} pages (${getMemAlgoName()})`); return; }

    const block = memBlocks[idx];
    if (block.size > pages) {
        const remainder = { start: block.start + pages, size: block.size - pages, free: true, pid: -1 };
        block.size = pages;
        memBlocks.splice(idx + 1, 0, remainder);
    }
    block.free = false;
    block.pid = pid;
    log('success', `[MemoryService] Allocated ${pages} pages at Frame ${block.start} for PID ${pid} (${getMemAlgoName()})`);
    sysLog.push({ t: elapsed(), msg: `Alloc ${pages}pg PID ${pid} (${getMemAlgoName()})` });
    updateUI();
}

function freeAllMemory(pid) {
    let freed = 0;
    memBlocks.forEach(b => { if (!b.free && b.pid === pid) { b.free = true; b.pid = -1; freed += b.size; }});
    coalesceMemory();
    if (freed > 0) {
        log('info', `[MemoryService] Freed ${freed} pages for PID ${pid}`);
        sysLog.push({ t: elapsed(), msg: `Freed ${freed}pg PID ${pid}` });
    }
    updateUI();
}

function coalesceMemory() {
    for (let i = 0; i < memBlocks.length - 1;) {
        if (memBlocks[i].free && memBlocks[i + 1].free) {
            memBlocks[i].size += memBlocks[i + 1].size;
            memBlocks.splice(i + 1, 1);
        } else i++;
    }
}

function getMemAlgoName() {
    return { FIRST: 'FIRST FIT', BEST: 'BEST FIT', WORST: 'WORST FIT' }[memAlgorithm];
}

// =============== FILE SYSTEM ===============

function createFile(name) {
    if (files[name]) { log('error', `[FileService] '${name}' already exists`); return; }
    files[name] = { name, content: '', owner: 1, readable: true, writable: true };
    log('success', `[FileService] Created '${name}'`);
    updateUI();
}

function readFile(name) {
    if (!files[name]) { log('error', `[FileService] '${name}' not found`); return; }
    if (!files[name].readable) { log('error', `[FileService] DENIED: '${name}' not readable`); return; }
    log('info', `[FileService] '${name}': ${files[name].content || '(empty)'}`);
}

function writeFile(name, data) {
    if (!files[name]) { log('error', `[FileService] '${name}' not found`); return; }
    if (!files[name].writable) { log('error', `[FileService] DENIED: '${name}' not writable`); return; }
    files[name].content = data;
    log('success', `[FileService] Written to '${name}'`);
}

function deleteFile(name) {
    if (!files[name]) { log('error', `[FileService] '${name}' not found`); return; }
    delete files[name];
    log('warning', `[FileService] Deleted '${name}'`);
    updateUI();
}

// =============== SECURITY ===============

function grantCap(pid, cap) {
    if (!capabilities[pid]) capabilities[pid] = new Set();
    capabilities[pid].add(cap);
    log('success', `[SecurityServer] GRANTED '${cap}' to PID ${pid}`);
    sysLog.push({ t: elapsed(), msg: `GRANTED ${cap} to PID ${pid}` });
}

function revokeCap(pid, cap) {
    if (capabilities[pid]) capabilities[pid].delete(cap);
    log('warning', `[SecurityServer] REVOKED '${cap}' from PID ${pid}`);
    sysLog.push({ t: elapsed(), msg: `REVOKED ${cap} from PID ${pid}` });
}

function hackFile(name) {
    log('kernel', `[Kernel] Routing message type='file'...`);
    log('sandbox', `[Sandbox] DENIED: PID 999 — unauthorized FILE operation! (no CAP_FILE)`);
    sysLog.push({ t: elapsed(), msg: 'DENIED PID 999 FILE access (hack attempt)' });
}

// =============== RESOURCES & DEADLOCK ===============

function lockResource(pid, name) {
    if (!resources[name]) { resources[name] = { name, heldBy: pid, waiters: [] }; log('info', `[ResourceMgr] PID ${pid} LOCKED '${name}'`); sysLog.push({ t: elapsed(), msg: `PID ${pid} locked '${name}'` }); updateUI(); return; }
    const r = resources[name];
    if (r.heldBy === -1) { r.heldBy = pid; log('info', `[ResourceMgr] PID ${pid} LOCKED '${name}'`); }
    else if (r.heldBy === pid) { log('info', `[ResourceMgr] PID ${pid} already holds '${name}'`); }
    else {
        r.waiters.push(pid);
        log('warning', `[ResourceMgr] PID ${pid} WAITING for '${name}' (held by PID ${r.heldBy})`);
        sysLog.push({ t: elapsed(), msg: `PID ${pid} waiting for '${name}' held by PID ${r.heldBy}` });
        if (detectDeadlock()) { log('error', `\n  !! DEADLOCK DETECTED !!\n`); sysLog.push({ t: elapsed(), msg: '*** DEADLOCK DETECTED ***' }); }
    }
    updateUI();
}

function unlockResource(pid, name) {
    if (!resources[name]) { log('error', `Resource '${name}' not found`); return; }
    const r = resources[name];
    if (r.heldBy !== pid) { log('error', `PID ${pid} doesn't hold '${name}'`); return; }
    r.heldBy = -1;
    log('info', `[ResourceMgr] PID ${pid} RELEASED '${name}'`);
    if (r.waiters.length > 0) {
        const next = r.waiters.shift();
        r.heldBy = next;
        log('success', `[ResourceMgr] '${name}' granted to PID ${next}`);
    }
    updateUI();
}

function detectDeadlock() {
    // Build wait-for graph
    const graph = {};
    for (const rn in resources) {
        const r = resources[rn];
        if (r.heldBy !== -1) {
            r.waiters.forEach(w => {
                if (!graph[w]) graph[w] = [];
                graph[w].push(r.heldBy);
            });
        }
    }
    // DFS cycle detection
    const visited = new Set(), inStack = new Set();
    function dfs(node) {
        visited.add(node); inStack.add(node);
        for (const nb of (graph[node] || [])) {
            if (inStack.has(nb)) { log('error', `  Deadlock cycle detected involving PID ${node} → PID ${nb}`); return true; }
            if (!visited.has(nb) && dfs(nb)) return true;
        }
        inStack.delete(node);
        return false;
    }
    const allNodes = new Set([...Object.keys(graph).map(Number), ...Object.values(graph).flat()]);
    for (const n of allNodes) { if (!visited.has(n) && dfs(n)) return true; }
    return false;
}

// =============== SEMAPHORES ===============

function semCreate(name, value) {
    semaphores[name] = { name, value, waitQueue: [] };
    log('info', `[Semaphore] Created '${name}' with value ${value}`);
    sysLog.push({ t: elapsed(), msg: `Semaphore '${name}' created val=${value}` });
}

function semWait(name, pid) {
    if (!semaphores[name]) { log('error', `Semaphore '${name}' not found`); return; }
    const s = semaphores[name];
    if (s.value > 0) {
        s.value--;
        log('success', `[Semaphore] PID ${pid} acquired '${name}' (val→${s.value})`);
    } else {
        s.waitQueue.push(pid);
        log('warning', `[Semaphore] PID ${pid} BLOCKED on '${name}' (val=0)`);
    }
    sysLog.push({ t: elapsed(), msg: `P(${name}) by PID ${pid}` });
}

function semSignal(name, pid) {
    if (!semaphores[name]) { log('error', `Semaphore '${name}' not found`); return; }
    const s = semaphores[name];
    if (s.waitQueue.length > 0) {
        const woken = s.waitQueue.shift();
        log('success', `[Semaphore] PID ${woken} woken from '${name}' by PID ${pid}`);
    } else {
        s.value++;
        log('info', `[Semaphore] V('${name}') by PID ${pid} (val→${s.value})`);
    }
    sysLog.push({ t: elapsed(), msg: `V(${name}) by PID ${pid}` });
}

// =============== IPC CHANNELS ===============

function ipcCreate(name, pid) {
    if (channels[name]) { log('error', `Channel '${name}' already exists`); return; }
    channels[name] = { name, owner: pid, buffer: [] };
    log('info', `[IPC] Channel '${name}' created by PID ${pid}`);
    sysLog.push({ t: elapsed(), msg: `Channel '${name}' created by PID ${pid}` });
}

function ipcSend(name, msg) {
    if (!channels[name]) { log('error', `Channel '${name}' not found`); return; }
    channels[name].buffer.push(msg);
    log('success', `[IPC] Sent to '${name}': "${msg}"`);
    sysLog.push({ t: elapsed(), msg: `IPC send '${name}': ${msg}` });
}

function ipcRecv(name) {
    if (!channels[name]) { log('error', `Channel '${name}' not found`); return; }
    if (channels[name].buffer.length === 0) { log('warning', `[IPC] Channel '${name}' empty`); return; }
    const msg = channels[name].buffer.shift();
    log('info', `[IPC] Received from '${name}': "${msg}"`);
    sysLog.push({ t: elapsed(), msg: `IPC recv '${name}': ${msg}` });
}

// =============== HELPERS ===============

function elapsed() {
    const ms = Date.now() - bootTime;
    const s = Math.floor(ms / 1000) % 60;
    const m = Math.floor(ms / 60000);
    return `${String(m).padStart(2,'0')}:${String(s).padStart(2,'0')}.${String(ms%1000).padStart(3,'0')}`;
}

// =============== UI CONTROLLER ===============

const consoleEl = document.getElementById('console-output');
const procContainer = document.getElementById('process-nodes-container');
const memBar = document.getElementById('memory-bar');
const memDetails = document.getElementById('memory-details');
const ganttTimeline = document.getElementById('gantt-timeline');
const modalOverlay = document.getElementById('modal-overlay');
let lastGanttLen = 0;

function log(type, msg) {
    const line = document.createElement('div');
    line.className = `console-line ${type}`;
    line.textContent = `[${elapsed()}] ${msg}`;
    consoleEl.appendChild(line);
    consoleEl.scrollTop = consoleEl.scrollHeight;
    document.getElementById('log-count').textContent = `${consoleEl.children.length} events`;
}

function updateUI() {
    updateProcessNodes();
    updateMemoryMap();
    updateGanttChart();
    updateStatusBar();
}

function updateProcessNodes() {
    const living = allProcesses.filter(p => p.state !== 'DEAD');
    const dead = allProcesses.filter(p => p.state === 'DEAD').slice(-4); // show last 4 dead

    const emptyEl = document.getElementById('proc-empty');
    if (living.length + dead.length === 0) {
        if (!emptyEl) {
            procContainer.innerHTML = `<div class="empty-state" id="proc-empty"><span class="empty-icon">⚡</span><p>No processes created yet</p><p class="hint">Click "+ Create" to begin</p></div>`;
        }
        document.getElementById('proc-count').textContent = '0 processes';
        return;
    }
    if (emptyEl) emptyEl.remove();

    const display = [...living, ...dead];
    document.getElementById('proc-count').textContent = `${living.length} active`;

    // Build nodes — only add new ones, update existing
    const existing = procContainer.querySelectorAll('.proc-node');
    const existingPids = new Set();
    existing.forEach(el => existingPids.add(parseInt(el.dataset.pid)));

    display.forEach(proc => {
        let node = procContainer.querySelector(`[data-pid="${proc.pid}"]`);
        if (!node) {
            node = document.createElement('div');
            node.className = 'proc-node';
            node.dataset.pid = proc.pid;
            node.innerHTML = `
                <div class="priority-badge">${proc.priority}</div>
                <div class="pid">P${proc.pid}</div>
                <div class="state-label">${proc.state}</div>
                <div class="burst-label">${proc.remaining}/${proc.burst}</div>`;
            procContainer.appendChild(node);
        } else {
            node.querySelector('.state-label').textContent = proc.state;
            node.querySelector('.burst-label').textContent = `${proc.remaining}/${proc.burst}`;
        }
        node.className = `proc-node ${proc.state.toLowerCase()}`;
    });

    // Remove nodes for processes no longer in display
    existing.forEach(el => {
        const pid = parseInt(el.dataset.pid);
        if (!display.find(p => p.pid === pid)) el.remove();
    });
}

function updateMemoryMap() {
    let usedPages = 0;
    memBar.innerHTML = '';
    memDetails.innerHTML = '';

    memBlocks.forEach((block, i) => {
        const pct = (block.size / TOTAL_PAGES * 100);
        const el = document.createElement('div');
        el.className = `mem-block ${block.free ? 'free' : 'used'}`;
        el.style.width = `${pct}%`;
        if (!block.free) {
            el.style.background = `linear-gradient(135deg, ${GANTT_COLORS[(block.pid - 100) % GANTT_COLORS.length]}66, ${GANTT_COLORS[(block.pid - 100) % GANTT_COLORS.length]}33)`;
            el.textContent = pct > 5 ? `P${block.pid}` : '';
            usedPages += block.size;
        } else {
            el.textContent = pct > 8 ? 'FREE' : '';
        }
        el.title = `Frame ${block.start} | ${block.size} pages | ${block.free ? 'FREE' : 'PID ' + block.pid}`;
        memBar.appendChild(el);
    });

    document.getElementById('mem-usage').textContent = `${usedPages} / ${TOTAL_PAGES} pages`;
}

function updateGanttChart() {
    if (ganttLog.length === 0) {
        ganttTimeline.innerHTML = '<div class="gantt-empty">Scheduling data will appear as processes execute</div>';
        return;
    }

    // Only add new entries
    for (let i = lastGanttLen; i < ganttLog.length; i++) {
        if (lastGanttLen === 0 && i === 0) ganttTimeline.innerHTML = '';
        const entry = ganttLog[i];
        const duration = entry.end - entry.start;
        const width = Math.max(36, Math.min(150, duration * 12));
        const block = document.createElement('div');
        block.className = 'gantt-block';
        block.style.width = `${width}px`;
        block.style.background = `linear-gradient(135deg, ${entry.color}cc, ${entry.color}66)`;
        block.style.border = `1px solid ${entry.color}`;
        block.innerHTML = `P${entry.pid}<span style="display:block;font-size:0.5rem;opacity:0.7">${duration}u Q=${entry.quantum || '?'}</span>`;
        block.dataset.time = `t=${entry.start}`;
        block.title = `PID ${entry.pid} | t=${entry.start}→${entry.end} | Duration: ${duration} units | Quantum: ${entry.quantum || '?'} | Algo: ${entry.algo || 'RR'}`;
        ganttTimeline.appendChild(block);
    }
    lastGanttLen = ganttLog.length;
    ganttTimeline.scrollLeft = ganttTimeline.scrollWidth;
    document.getElementById('gantt-algo').textContent = `${getAlgoName()} (Base Q=${timeQuantum}, Effective Q=${lastEffectiveQ})`;
}

function updateStatusBar() {
    document.getElementById('sched-chip').textContent = { RR: 'RR', PRIORITY: 'PRI', SJF: 'SJF' }[algorithm];
    document.getElementById('mem-chip').textContent = getMemAlgoName();
    document.getElementById('time-chip').textContent = `t=${cpuTime}`;
    if (currentRunning) {
        document.getElementById('cpu-chip').textContent = `CPU: P${currentRunning.pid}`;
        document.getElementById('cpu-chip').className = 'status-chip running';
    } else {
        document.getElementById('cpu-chip').textContent = 'CPU: IDLE';
        document.getElementById('cpu-chip').className = 'status-chip';
    }
}

// =============== MODAL SYSTEM ===============

function showModal(title, fields, callback) {
    document.getElementById('modal-title').textContent = title;
    const container = document.getElementById('modal-fields');
    container.innerHTML = '';

    fields.forEach(f => {
        const div = document.createElement('div');
        div.className = 'modal-field';
        if (f.type === 'select') {
            div.innerHTML = `<label>${f.label}</label><select id="modal-${f.id}">${f.options.map(o => `<option value="${o.value}">${o.label}</option>`).join('')}</select>`;
        } else {
            div.innerHTML = `<label>${f.label}</label><input type="${f.type || 'text'}" id="modal-${f.id}" value="${f.default || ''}" placeholder="${f.placeholder || ''}">`;
        }
        container.appendChild(div);
    });

    modalOverlay.classList.add('show');

    document.getElementById('modal-confirm').onclick = () => {
        const values = {};
        fields.forEach(f => { values[f.id] = document.getElementById(`modal-${f.id}`).value; });
        modalOverlay.classList.remove('show');
        callback(values);
    };
    document.getElementById('modal-cancel').onclick = () => modalOverlay.classList.remove('show');

    // Focus first input
    setTimeout(() => { const inp = container.querySelector('input,select'); if (inp) inp.focus(); }, 100);
}

// =============== COMMAND HANDLERS ===============

const commands = {
    create_process: () => showModal('Create Process', [
        { id: 'burst', label: 'Burst Time', type: 'number', default: '30', placeholder: '1-9999' },
        { id: 'priority', label: 'Priority (1=highest, 10=lowest)', type: 'number', default: '5', placeholder: '1-10' }
    ], v => createProcess(parseInt(v.burst) || 30, Math.max(1, Math.min(10, parseInt(v.priority) || 5)))),

    list_process: () => {
        const procs = allProcesses.filter(p => p.state !== 'DEAD');
        if (procs.length === 0) { log('info', 'No active processes'); return; }
        procs.forEach(p => log('info', `  PID ${p.pid} | ${p.name} | Burst: ${p.burst} | Remaining: ${p.remaining} | Pri: ${p.priority} | ${p.state}`));
    },

    ps: () => {
        log('kernel', `  Algorithm: ${getAlgoName()} | Quantum: ${timeQuantum}`);
        const snapshot = [currentRunning, ...readyList, ...blockedList].filter(Boolean);
        if (snapshot.length === 0) { log('info', '  No processes in scheduler'); return; }
        snapshot.forEach(p => log('info', `  P${p.pid} | Burst: ${p.burst} | Rem: ${p.remaining} | Pri: ${p.priority} | ${p.state}`));
    },

    kill: () => showModal('Kill Process', [
        { id: 'pid', label: 'Process PID', type: 'number', placeholder: '100' }
    ], v => killProcess(parseInt(v.pid))),

    suspend: () => showModal('Suspend Process', [
        { id: 'pid', label: 'Process PID', type: 'number', placeholder: '100' }
    ], v => suspendProcess(parseInt(v.pid))),

    resume: () => showModal('Resume Process', [
        { id: 'pid', label: 'Process PID', type: 'number', placeholder: '100' }
    ], v => resumeProcess(parseInt(v.pid))),

    set_rr: () => { algorithm = 'RR'; log('info', '[Scheduler] Algorithm → Round Robin'); document.querySelectorAll('#sched-cmds .cmd-btn').forEach(b => b.classList.remove('active')); document.querySelector('[data-cmd="set_rr"]').classList.add('active'); updateUI(); },
    set_priority: () => { algorithm = 'PRIORITY'; log('info', '[Scheduler] Algorithm → Priority'); document.querySelectorAll('#sched-cmds .cmd-btn').forEach(b => b.classList.remove('active')); document.querySelector('[data-cmd="set_priority"]').classList.add('active'); updateUI(); },
    set_sjf: () => { algorithm = 'SJF'; log('info', '[Scheduler] Algorithm → SJF'); document.querySelectorAll('#sched-cmds .cmd-btn').forEach(b => b.classList.remove('active')); document.querySelector('[data-cmd="set_sjf"]').classList.add('active'); updateUI(); },

    tick: () => { log('kernel', '[Kernel] Timer interrupt → tick'); schedulerTick(); },

    gantt: () => {
        if (ganttLog.length === 0) { log('info', 'No scheduling data yet'); return; }
        log('info', `  Gantt: ${ganttLog.map(e => `P${e.pid}(${e.start}-${e.end})`).join(' → ')}`);
    },

    schedstat: () => {
        log('kernel', `  ═══ Scheduler Stats ═══`);
        log('info', `  Algorithm: ${getAlgoName()} | Quantum: ${timeQuantum}`);
        log('info', `  CPU Time: ${cpuTime} | Switches: ${contextSwitches} | Completed: ${completions}`);
        log('info', `  Ready: ${readyList.length} | Blocked: ${blockedList.length} | Running: ${currentRunning ? 'P' + currentRunning.pid : 'IDLE'}`);
    },

    alloc: () => showModal('Allocate Memory', [
        { id: 'pid', label: 'Process PID', type: 'number', placeholder: '100' },
        { id: 'bytes', label: 'Bytes', type: 'number', default: '8192', placeholder: '4096' }
    ], v => { if (!capabilities[parseInt(v.pid)]?.has('CAP_MEM')) { log('sandbox', `[Sandbox] DENIED: PID ${v.pid} — no CAP_MEM`); return; } allocMemory(parseInt(v.pid), parseInt(v.bytes)); }),

    free_mem: () => showModal('Free Memory', [
        { id: 'pid', label: 'Process PID', type: 'number', placeholder: '100' }
    ], v => freeAllMemory(parseInt(v.pid))),

    set_first: () => { memAlgorithm = 'FIRST'; log('info', '[MemoryService] Algorithm → FIRST FIT'); document.querySelectorAll('#mem-cmds .cmd-btn').forEach(b => b.classList.remove('active')); document.querySelector('[data-cmd="set_first"]').classList.add('active'); },
    set_best: () => { memAlgorithm = 'BEST'; log('info', '[MemoryService] Algorithm → BEST FIT'); document.querySelectorAll('#mem-cmds .cmd-btn').forEach(b => b.classList.remove('active')); document.querySelector('[data-cmd="set_best"]').classList.add('active'); },
    set_worst: () => { memAlgorithm = 'WORST'; log('info', '[MemoryService] Algorithm → WORST FIT'); document.querySelectorAll('#mem-cmds .cmd-btn').forEach(b => b.classList.remove('active')); document.querySelector('[data-cmd="set_worst"]').classList.add('active'); },

    memmap: () => {
        log('kernel', `  ═══ Memory Map (${TOTAL_PAGES} pages, ${TOTAL_PAGES * 4}KB) — ${getMemAlgoName()} ═══`);
        memBlocks.forEach(b => log('info', `  Frame ${b.start} | ${b.size}pg | ${b.free ? 'FREE' : 'PID ' + b.pid}`));
    },

    create_file: () => showModal('Create File', [{ id: 'name', label: 'Filename', placeholder: 'myfile.txt' }], v => createFile(v.name)),
    read_file: () => {
        const fNames = Object.keys(files);
        if (fNames.length === 0) { log('error', 'No files exist. Create one first.'); return; }
        showModal('Read File', [
            { id: 'name', label: 'Select File', type: 'select', options: fNames.map(f => ({value: f, label: `${f} (${files[f].content.length}B)`})) }
        ], v => readFile(v.name));
    },
    write_file: () => {
        const fNames = Object.keys(files);
        if (fNames.length === 0) { log('error', 'No files exist. Create one first.'); return; }
        showModal('Write to File', [
            { id: 'name', label: 'Select File', type: 'select', options: fNames.map(f => ({value: f, label: `${f} ${files[f].writable?'✅':'🔒'}`})) },
            { id: 'data', label: 'Content', placeholder: 'Hello World' }
        ], v => writeFile(v.name, v.data));
    },
    delete_file: () => {
        const fNames = Object.keys(files);
        if (fNames.length === 0) { log('error', 'No files to delete'); return; }
        showModal('Delete File', [
            { id: 'name', label: 'Select File', type: 'select', options: fNames.map(f => ({value: f, label: f})) }
        ], v => deleteFile(v.name));
    },
    chmod_file: () => {
        const fNames = Object.keys(files);
        if (fNames.length === 0) { log('error', 'No files exist'); return; }
        showModal('Change Permissions', [
            { id: 'name', label: 'Select File', type: 'select', options: fNames.map(f => ({value: f, label: `${f} (R:${files[f].readable?'Y':'N'} W:${files[f].writable?'Y':'N'})`})) },
            { id: 'perm', label: 'Permission', type: 'select', options: [{value:'both',label:'Read+Write'},{value:'read',label:'Read Only'},{value:'write',label:'Write Only'},{value:'none',label:'None'}] }
        ], v => { if (!files[v.name]) { log('error','File not found'); return; } files[v.name].readable = v.perm === 'read' || v.perm === 'both'; files[v.name].writable = v.perm === 'write' || v.perm === 'both'; log('success', `[FileService] '${v.name}' perms: R=${files[v.name].readable ? 'yes' : 'no'} W=${files[v.name].writable ? 'yes' : 'no'}`); });
    },
    ls: () => { const names = Object.keys(files); if (names.length === 0) { log('info', 'No files'); return; } names.forEach(n => { const f = files[n]; log('info', `  ${n} | Owner: PID ${f.owner} | R:${f.readable?'Y':'N'} W:${f.writable?'Y':'N'} | ${f.content.length}B`); }); },

    grant: () => {
        const procs = allProcesses.filter(p => p.state !== 'DEAD');
        if (procs.length === 0) { log('error', 'No active processes to grant capabilities to'); return; }
        showModal('Grant Capability', [
            { id: 'pid', label: 'Process', type: 'select', options: procs.map(p => ({value: String(p.pid), label: `PID ${p.pid} (${p.name})`})) },
            { id: 'cap', label: 'Capability', type: 'select', options: [{value:'CAP_FILE',label:'CAP_FILE (File Access)'},{value:'CAP_MEM',label:'CAP_MEM (Memory Access)'}] }
        ], v => grantCap(parseInt(v.pid), v.cap));
    },

    revoke: () => {
        const procs = allProcesses.filter(p => p.state !== 'DEAD');
        if (procs.length === 0) { log('error', 'No active processes'); return; }
        showModal('Revoke Capability', [
            { id: 'pid', label: 'Process', type: 'select', options: procs.map(p => ({value: String(p.pid), label: `PID ${p.pid} — caps: ${capabilities[p.pid] ? [...capabilities[p.pid]].join(', ') : 'none'}`})) },
            { id: 'cap', label: 'Capability to Revoke', type: 'select', options: [{value:'CAP_FILE',label:'CAP_FILE (File Access)'},{value:'CAP_MEM',label:'CAP_MEM (Memory Access)'}] }
        ], v => revokeCap(parseInt(v.pid), v.cap));
    },

    capabilities: () => {
        log('kernel', '  ═══ Capability Table ═══');
        const procs = allProcesses.filter(p => p.state !== 'DEAD');
        if (procs.length === 0) { log('info', '  No active processes'); }
        log('info', `  PID 1 (Shell): ${[...capabilities[1]].join(', ')}`);
        procs.forEach(p => {
            const caps = capabilities[p.pid] ? [...capabilities[p.pid]].join(', ') : 'NONE';
            log('info', `  PID ${p.pid} (${p.state}): ${caps}`);
        });
    },

    hack_file: () => {
        const fileNames = Object.keys(files);
        if (fileNames.length === 0) {
            log('kernel', `[Kernel] Routing message type='file'...`);
            log('sandbox', `[Sandbox] DENIED: PID 999 — unauthorized FILE operation! (no CAP_FILE)`);
            sysLog.push({ t: elapsed(), msg: 'DENIED PID 999 FILE access (hack attempt)' });
            return;
        }
        showModal('🏴‍☠️ Hack Attempt — Unauthorized Access', [
            { id: 'name', label: 'Target File', type: 'select', options: fileNames.map(f => ({value: f, label: `${f} (${files[f].readable?'R':''}${files[f].writable?'W':''})`})) }
        ], v => {
            log('kernel', `[Kernel] Routing message type='file' for '${v.name}'...`);
            log('kernel', `[Kernel] Capability check: PID 999 → CAP_FILE?`);
            log('sandbox', `[Sandbox] DENIED: PID 999 has NO CAP_FILE — access to '${v.name}' BLOCKED!`);
            log('error', `[SecurityServer] Unauthorized access attempt logged for '${v.name}'`);
            sysLog.push({ t: elapsed(), msg: `DENIED PID 999 access to '${v.name}' (hack attempt)` });
        });
    },

    lock: () => showModal('Lock Resource', [
        { id: 'pid', label: 'Process PID', type: 'number', placeholder: '100' },
        { id: 'res', label: 'Resource Name', placeholder: 'fileA' }
    ], v => lockResource(parseInt(v.pid), v.res)),

    unlock: () => showModal('Unlock Resource', [
        { id: 'pid', label: 'Process PID', type: 'number', placeholder: '100' },
        { id: 'res', label: 'Resource Name', placeholder: 'fileA' }
    ], v => unlockResource(parseInt(v.pid), v.res)),

    deadlock: () => { if (!detectDeadlock()) log('success', '  No deadlock detected. System is safe.'); },

    resources: () => {
        log('kernel', '  ═══ Resource Table ═══');
        const rNames = Object.keys(resources);
        if (rNames.length === 0) { log('info', '  No resources locked'); return; }
        rNames.forEach(n => { const r = resources[n]; log('info', `  ${n} | Held: PID ${r.heldBy} | Waiters: ${r.waiters.length > 0 ? r.waiters.map(w => 'PID ' + w).join(', ') : 'none'}`); });
    },

    sem_create: () => showModal('Create Semaphore', [
        { id: 'name', label: 'Name', placeholder: 'mutex1' },
        { id: 'value', label: 'Initial Value', type: 'number', default: '1' }
    ], v => semCreate(v.name, parseInt(v.value) || 1)),

    sem_wait: () => showModal('Semaphore P()', [
        { id: 'name', label: 'Semaphore Name', placeholder: 'mutex1' },
        { id: 'pid', label: 'Process PID', type: 'number', placeholder: '100' }
    ], v => semWait(v.name, parseInt(v.pid))),

    sem_signal: () => showModal('Semaphore V()', [
        { id: 'name', label: 'Semaphore Name', placeholder: 'mutex1' },
        { id: 'pid', label: 'Process PID', type: 'number', placeholder: '100' }
    ], v => semSignal(v.name, parseInt(v.pid))),

    ipc_create: () => showModal('Create IPC Channel', [
        { id: 'name', label: 'Channel Name', placeholder: 'data_pipe' },
        { id: 'pid', label: 'Owner PID', type: 'number', placeholder: '100' }
    ], v => ipcCreate(v.name, parseInt(v.pid))),

    ipc_send: () => showModal('Send to Channel', [
        { id: 'name', label: 'Channel Name', placeholder: 'data_pipe' },
        { id: 'msg', label: 'Message', placeholder: 'Hello from process!' }
    ], v => ipcSend(v.name, v.msg)),

    ipc_recv: () => showModal('Receive from Channel', [
        { id: 'name', label: 'Channel Name', placeholder: 'data_pipe' }
    ], v => ipcRecv(v.name)),

    ipc_list: () => {
        log('kernel', '  ═══ IPC Channels ═══');
        const names = Object.keys(channels);
        if (names.length === 0) { log('info', '  No channels'); return; }
        names.forEach(n => { const c = channels[n]; log('info', `  ${n} | Owner: PID ${c.owner} | Buffered: ${c.buffer.length}`); });
    },

    syslog: () => {
        log('kernel', `  ═══ System Log (last 20) ═══`);
        sysLog.slice(-20).forEach(e => log('boot', `  [${e.t}] ${e.msg}`));
        log('info', `  Total entries: ${sysLog.length}`);
    },

    kill_service: () => {
        log('error', '[WATCHDOG] CRITICAL FAULT: FileService crashed!');
        log('success', '[WATCHDOG] Restarting FileService... OK');
        sysLog.push({ t: elapsed(), msg: 'FileService crashed & restarted' });
    },

    clear: () => { consoleEl.innerHTML = ''; },
};

// =============== EVENT WIRING ===============

// Command buttons
document.querySelectorAll('.cmd-btn').forEach(btn => {
    btn.addEventListener('click', () => {
        const cmd = btn.dataset.cmd;
        if (commands[cmd]) commands[cmd]();
    });
});

// Auto-tick toggle
document.getElementById('btn-auto-tick').addEventListener('click', function () {
    if (autoTickInterval) {
        clearInterval(autoTickInterval);
        autoTickInterval = null;
        this.textContent = '▶ Auto';
        this.classList.remove('active');
        log('info', '[Scheduler] Auto-tick stopped');
    } else {
        autoTickInterval = setInterval(() => schedulerTick(), 1000);
        this.textContent = '⏸ Stop';
        this.classList.add('active');
        log('info', '[Scheduler] Auto-tick started (1 tick/sec)');
    }
});

// Sidebar group toggle
document.querySelectorAll('.cmd-group-title').forEach(title => {
    title.addEventListener('click', () => {
        const target = document.getElementById(title.dataset.toggle);
        if (target) target.style.display = target.style.display === 'none' ? 'flex' : 'none';
    });
});

// Modal close on overlay click
modalOverlay.addEventListener('click', e => { if (e.target === modalOverlay) modalOverlay.classList.remove('show'); });

// Keyboard: Escape closes modal
document.addEventListener('keydown', e => { if (e.key === 'Escape') modalOverlay.classList.remove('show'); });

// Initial UI
updateUI();
