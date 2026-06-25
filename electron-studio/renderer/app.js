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
        } else {
            setStatus('Connection failed');
            log('error', `Connection failed: ${result.error}`);
        }
    } catch (e) {
        setStatus('Connection error');
        log('error', `Connection error: ${e.message}`);
    }
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
        const fs = require('fs');
        const content = fs.readFileSync(result.filePaths[0], 'utf-8');
        $('#script-editor').value = content;
        $('#script-file').textContent = result.filePaths[0].split(/[/\\]/).pop();
        log('info', `Opened: ${result.filePaths[0]}`);
    }
}

function handleSave() {
    /* TODO: implement save dialog */
    log('info', 'Save not yet implemented');
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
