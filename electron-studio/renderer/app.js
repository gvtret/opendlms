/**
 * \file app.js
 * \brief OpenDLMS Studio — Renderer process
 */

/* global openDLMS */

const $ = (sel) => document.querySelector(sel);
const $$ = (sel) => document.querySelectorAll(sel);

let connected = false;

/* ── Init ────────────────────────────────────────────────────────────────── */

async function init() {
    const version = await openDLMS.getVersion();
    $('#version').textContent = `v${version}`;
    document.title = `OpenDLMS Studio ${version}`;

    setupTabs();
    setupButtons();
    setupKeyboard();
    setupConsole();
}

/* ── Tabs ────────────────────────────────────────────────────────────────── */

function setupTabs() {
    $$('.tab').forEach((tab) => {
        tab.addEventListener('click', () => {
            $$('.tab').forEach((t) => t.classList.remove('active'));
            $$('.tab-panel').forEach((p) => p.classList.remove('active'));

            tab.classList.add('active');
            $(`#tab-${tab.dataset.tab}`).classList.add('active');
        });
    });
}

/* ── Buttons ─────────────────────────────────────────────────────────────── */

function setupButtons() {
    /* Connection */
    $('#btn-connect').addEventListener('click', handleConnect);
    $('#btn-disconnect').addEventListener('click', handleDisconnect);

    /* Script */
    $('#btn-run').addEventListener('click', handleRun);
    $('#btn-stop').addEventListener('click', handleStop);
    $('#btn-open').addEventListener('click', handleOpen);
    $('#btn-save').addEventListener('click', handleSave);

    /* Manual requests */
    $('#btn-get').addEventListener('click', handleGet);
    $('#btn-set').addEventListener('click', handleSet);

    /* Log */
    $('#btn-clear-log').addEventListener('click', () => {
        $('#packet-log').innerHTML = '';
    });
}

/* ── Keyboard shortcuts ──────────────────────────────────────────────────── */

function setupKeyboard() {
    document.addEventListener('keydown', (e) => {
        if (e.key === 'F5') {
            e.preventDefault();
            handleRun();
        }
        if (e.key === 'F10') {
            e.preventDefault();
            handleStop();
        }
        if (e.ctrlKey && e.key === 's') {
            e.preventDefault();
            handleSave();
        }
    });
}

/* ── Console ─────────────────────────────────────────────────────────────── */

let consoleHistory = [];
let consoleHistoryIndex = -1;

async function setupConsole() {
    const input = $('#console-input');

    /* Load persistent history */
    try {
        consoleHistory = await openDLMS.getHistory();
        consoleHistoryIndex = consoleHistory.length;
    } catch (e) {
        consoleHistory = [];
        consoleHistoryIndex = 0;
    }

    /* Input handler */
    input.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') {
            e.preventDefault();
            const cmd = input.value.trim();
            if (cmd) {
                consoleHistory.push(cmd);
                consoleHistoryIndex = consoleHistory.length;
                openDLMS.addToHistory(cmd);
                executeConsoleCommand(cmd);
                input.value = '';
            }
        } else if (e.key === 'ArrowUp') {
            e.preventDefault();
            if (consoleHistoryIndex > 0) {
                consoleHistoryIndex--;
                input.value = consoleHistory[consoleHistoryIndex] || '';
            }
        } else if (e.key === 'ArrowDown') {
            e.preventDefault();
            if (consoleHistoryIndex < consoleHistory.length - 1) {
                consoleHistoryIndex++;
                input.value = consoleHistory[consoleHistoryIndex] || '';
            } else {
                consoleHistoryIndex = consoleHistory.length;
                input.value = '';
            }
        } else if (e.key === 'l' && e.ctrlKey) {
            e.preventDefault();
            clearConsole();
        }
    });

    /* Clear buttons */
    $('#btn-console-clear').addEventListener('click', clearConsole);
    $('#btn-console-clear-history').addEventListener('click', async () => {
        consoleHistory = [];
        consoleHistoryIndex = -1;
        await openDLMS.clearHistory();
        consolePrint('History cleared', 'info');
    });

    consolePrint(`Lua Console ready. ${consoleHistory.length} commands in history.`, 'info');
}

async function executeConsoleCommand(cmd) {
    /* Echo input */
    consolePrint(`>>> ${cmd}`, 'input');

    try {
        /* Execute command */
        const result = await openDLMS.execReturn(cmd);

        /* Capture print output */
        const output = await openDLMS.getOutput();
        if (output && output.length > 0) {
            output.split('\n').forEach((line) => {
                if (line.trim()) consolePrint(line, 'output');
            });
        }
        await openDLMS.clearOutput();

        /* Show result if not empty */
        if (result.data && result.data.length > 0) {
            consolePrint(result.data, 'output');
        } else if (result.success) {
            /* Statement executed successfully, no return value */
            /* Don't show anything for silent success */
        }

        /* Show error if any */
        if (result.error && result.error.length > 0) {
            consolePrint(`Error: ${result.error}`, 'error');
        }
    } catch (e) {
        consolePrint(`Error: ${e.message}`, 'error');
    }
}

function consolePrint(text, type = 'output') {
    const output = $('#console-output');
    const line = document.createElement('div');
    line.className = `console-line console-line-${type}`;
    line.textContent = text;
    output.appendChild(line);

    if ($('#console-auto-scroll').checked) {
        output.scrollTop = output.scrollHeight;
    }
}

function clearConsole() {
    $('#console-output').innerHTML = '';
}

/* ── Connection ──────────────────────────────────────────────────────────── */

async function handleConnect() {
    const host = $('#host').value;
    const port = parseInt($('#port').value, 10);

    setStatus('Connecting...');
    log('info', `Connecting to ${host}:${port}...`);

    try {
        const result = await openDLMS.execScript(`connect("${host}", ${port})`);
        if (result.success) {
            connected = true;
            $('#btn-connect').disabled = true;
            $('#btn-disconnect').disabled = false;
            $('#status-connection').textContent = `Connected to ${host}:${port}`;
            setStatus('Connected');
            log('info', `Connected to ${host}:${port}`);

            /* Populate object tree */
            await refreshObjectTree();
        } else {
            setStatus('Connection failed');
            log('error', `Connection failed: ${result.error}`);
        }
    } catch (e) {
        setStatus('Connection error');
        log('error', `Connection error: ${e.message}`);
    }
}

async function refreshObjectTree() {
    const tree = $('#object-tree');
    tree.innerHTML = '<div class="tree-empty">Loading objects...</div>';

    try {
        const result = await openDLMS.getObjectList();
        if (result.success && result.data) {
            /* Parse AXDR-encoded object list */
            const objects = parseObjectList(result.data);
            if (objects.length > 0) {
                tree.innerHTML = '';
                objects.forEach((obj) => {
                    const item = document.createElement('div');
                    item.className = 'tree-item';
                    item.textContent = `${obj.classId} ${obj.obis} (${obj.version})`;
                    item.title = `Class ID: ${obj.classId}, OBIS: ${obj.obis}`;
                    item.addEventListener('click', () => {
                        /* Select object for GET/SET */
                        log('info', `Selected: ${obj.classId} ${obj.obis}`);
                    });
                    tree.appendChild(item);
                });
                log('info', `Found ${objects.length} objects`);
            } else {
                tree.innerHTML = '<div class="tree-empty">No objects found</div>';
            }
        } else {
            tree.innerHTML = `<div class="tree-empty">${result.error || 'Failed to load'}</div>`;
        }
    } catch (e) {
        tree.innerHTML = `<div class="tree-empty">Error: ${e.message}</div>`;
    }
}

function parseObjectList(hexStr) {
    /* Simple AXDR object list parser */
    const objects = [];
    const bytes = [];
    for (let i = 0; i < hexStr.length; i += 2) {
        bytes.push(parseInt(hexStr.substring(i, i + 2), 16));
    }

    let pos = 0;

    /* Skip tag (0x01 = array) and length */
    if (bytes[pos] === 0x01) pos++;
    const count = bytes[pos++];

    for (let i = 0; i < count && pos < bytes.length; i++) {
        /* Skip structure tag (0x02) and count (0x04) */
        if (bytes[pos] === 0x02) pos++;
        if (bytes[pos] === 0x04) pos++;

        /* Class ID (2 bytes) */
        const classId = (bytes[pos] << 8) | bytes[pos + 1];
        pos += 2;

        /* OBIS (6 bytes) */
        const obis = `${bytes[pos]}.${bytes[pos+1]}.${bytes[pos+2]}.${bytes[pos+3]}.${bytes[pos+4]}.${bytes[pos+5]}`;
        pos += 6;

        /* Version (2 bytes) */
        const version = (bytes[pos] << 8) | bytes[pos + 1];
        pos += 2;

        objects.push({ classId, obis, version });
    }

    return objects;
}

async function handleDisconnect() {
    try {
        await openDLMS.disconnect();
    } catch (e) {
        /* ignore */
    }
    connected = false;
    $('#btn-connect').disabled = false;
    $('#btn-disconnect').disabled = true;
    $('#status-connection').textContent = 'Disconnected';
    setStatus('Disconnected');
    log('info', 'Disconnected');
}

/* ── Script execution ────────────────────────────────────────────────────── */

async function handleRun() {
    const script = $('#script-editor').value;
    if (!script.trim()) {
        log('error', 'No script to run');
        return;
    }

    log('info', 'Running script...');
    setStatus('Running...');
    $('#btn-run').disabled = true;
    $('#btn-stop').disabled = false;

    try {
        const result = await openDLMS.execScript(script);

        /* Capture any Lua print output */
        const output = await openDLMS.getOutput();
        if (output && output.length > 0) {
            output.split('\n').forEach((line) => {
                if (line.trim()) log('lua', line);
            });
        }
        await openDLMS.clearOutput();

        if (result.success) {
            log('info', 'Script completed');
            setStatus('Ready');
        } else {
            log('error', `Script error: ${result.error}`);
            setStatus('Script error');
        }
    } catch (e) {
        log('error', `Script error: ${e.message}`);
        setStatus('Script error');
    }

    $('#btn-run').disabled = false;
    $('#btn-stop').disabled = true;
}

function handleStop() {
    log('info', 'Script stopped');
    setStatus('Ready');
    $('#btn-run').disabled = false;
    $('#btn-stop').disabled = true;
}

/* ── File operations ─────────────────────────────────────────────────────── */

async function handleOpen() {
    const result = await openDLMS.openFile();
    if (!result.canceled && result.filePaths.length > 0) {
        /* File content is read by main process via Lua execFile */
        const script = `io.open("${result.filePaths[0].replace(/\\/g, '\\\\')}", "r"):read("*a")`;
        /* Actually, we need to read the file content in main process */
        /* Use a different approach: exec the file directly */
        const execResult = await openDLMS.execFile(result.filePaths[0]);
        if (execResult.success) {
            log('info', `Loaded: ${result.filePaths[0]}`);
        } else {
            log('error', `Failed to load: ${execResult.error}`);
        }
    }
}

async function handleSave() {
    const content = $('#script-editor').value;
    const defaultPath = $('#script-file').textContent;

    const result = await openDLMS.saveFile({
        defaultPath: defaultPath === 'Untitled' ? 'script.lua' : defaultPath,
        content,
    });

    if (result.success) {
        $('#script-file').textContent = result.path.split(/[/\\]/).pop();
        log('info', `Saved: ${result.path}`);
    } else {
        log('info', 'Save cancelled');
    }
}

/* ── Manual GET/SET ──────────────────────────────────────────────────────── */

function parseObis(str) {
    return str.split('.').map(Number);
}

async function handleGet() {
    const classId = parseInt($('#get-class').value, 10);
    const obis = parseObis($('#get-obis').value);
    const attrId = parseInt($('#get-attr').value, 10);

    if (obis.length !== 6 || obis.some(isNaN)) {
        $('#get-result').textContent = 'Invalid OBIS code';
        return;
    }

    setStatus('Sending GET...');
    log('info', `GET class=${classId} obis=${$('#get-obis').value} attr=${attrId}`);

    try {
        const result = await openDLMS.clientGet({
            invokeId: 1,
            classId,
            obis,
            attrId,
        });

        if (result) {
            const hex = Array.from(result).map((b) => b.toString(16).padStart(2, '0')).join(' ');
            $('#get-result').textContent = hex;
            log('rx', `GET response: ${hex}`);
        } else {
            $('#get-result').textContent = 'No response';
        }
    } catch (e) {
        $('#get-result').textContent = `Error: ${e.message}`;
        log('error', `GET error: ${e.message}`);
    }

    setStatus('Ready');
}

async function handleSet() {
    const classId = parseInt($('#set-class').value, 10);
    const obis = parseObis($('#set-obis').value);
    const attrId = parseInt($('#set-attr').value, 10);
    const dataHex = $('#set-data').value.replace(/\s+/g, '');

    if (obis.length !== 6 || obis.some(isNaN)) {
        $('#set-result').textContent = 'Invalid OBIS code';
        return;
    }

    const data = [];
    for (let i = 0; i < dataHex.length; i += 2) {
        data.push(parseInt(dataHex.substring(i, i + 2), 16));
    }

    setStatus('Sending SET...');
    log('info', `SET class=${classId} obis=${$('#set-obis').value} attr=${attrId} data=${dataHex}`);

    try {
        const result = await openDLMS.clientSet({
            invokeId: 2,
            classId,
            obis,
            attrId,
            data: Uint8Array.from(data),
        });

        if (result) {
            const hex = Array.from(result).map((b) => b.toString(16).padStart(2, '0')).join(' ');
            $('#set-result').textContent = hex;
            log('rx', `SET response: ${hex}`);
        } else {
            $('#set-result').textContent = 'No response';
        }
    } catch (e) {
        $('#set-result').textContent = `Error: ${e.message}`;
        log('error', `SET error: ${e.message}`);
    }

    setStatus('Ready');
}

/* ── Logging ─────────────────────────────────────────────────────────────── */

function log(type, message) {
    const logEl = $('#packet-log');
    const time = new Date().toLocaleTimeString();
    const entry = document.createElement('div');
    entry.className = 'log-entry';
    entry.innerHTML = `<span class="log-time">[${time}]</span> <span class="log-${type}">${message}</span>`;
    logEl.appendChild(entry);

    if ($('#auto-scroll').checked) {
        logEl.scrollTop = logEl.scrollHeight;
    }
}

/* ── Status bar ──────────────────────────────────────────────────────────── */

function setStatus(text) {
    $('#status-ready').textContent = text;
}

/* ── Start ───────────────────────────────────────────────────────────────── */

init();
