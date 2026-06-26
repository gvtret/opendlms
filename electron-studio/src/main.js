/**
 * \file main.js
 * \brief Electron.js main process for OpenDLMS Studio
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

const { app, BrowserWindow, ipcMain, dialog } = require('electron');
const path = require('path');
const fs = require('fs');

/* Load native addon */
let native;
try {
    native = require('../../native/build/Release/opendlms-native.node');
} catch (e) {
    console.error('Failed to load native addon:', e.message);
    console.error('Run: npm run build:native');
    process.exit(1);
}

let mainWindow = null;
let luaBridge = null;

function createWindow() {
    mainWindow = new BrowserWindow({
        width: 1200,
        height: 800,
        title: 'OpenDLMS Studio',
        webPreferences: {
            nodeIntegration: false,
            contextIsolation: true,
            preload: path.join(__dirname, 'preload.js'),
        },
    });

    mainWindow.loadFile(path.join(__dirname, '..', 'renderer', 'index.html'));

    mainWindow.on('closed', () => {
        mainWindow = null;
    });

    /* Create Lua bridge */
    luaBridge = new native.LuaBridge();
}

/* ── IPC Handlers ────────────────────────────────────────────────────────── */

ipcMain.handle('get-version', () => {
    return native.VERSION;
});

/* ── Lua bridge IPC ──────────────────────────────────────────────────────── */

ipcMain.handle('lua:exec', (event, script) => {
    if (!luaBridge) {
        return { success: false, error: 'Lua bridge not initialized' };
    }

    const error = luaBridge.exec(script);
    if (error) {
        return { success: false, error };
    }
    return { success: true };
});

ipcMain.handle('lua:execFile', (event, filename) => {
    if (!luaBridge) {
        return { success: false, error: 'Lua bridge not initialized' };
    }

    try {
        const content = fs.readFileSync(filename, 'utf-8');
        const error = luaBridge.exec(content);
        if (error) {
            return { success: false, error };
        }
        return { success: true };
    } catch (e) {
        return { success: false, error: e.message };
    }
});

ipcMain.handle('lua:isConnected', () => {
    if (!luaBridge) return false;
    return luaBridge.isConnected();
});

ipcMain.handle('lua:getError', () => {
    if (!luaBridge) return '';
    return luaBridge.getError();
});

ipcMain.handle('lua:disconnect', () => {
    if (!luaBridge) return { success: false };
    const error = luaBridge.exec('disconnect()');
    return { success: !error, error };
});

/* ── Direct client API (for manual GET/SET) ──────────────────────────────── */

ipcMain.handle('client:get', (event, { invokeId, classId, obis, attrId }) => {
    if (!luaBridge) {
        return { success: false, error: 'Bridge not initialized' };
    }

    const obisStr = obis.join('.');
    const script = `hex(getCosem(${classId}, obis("${obisStr}"), ${attrId}))`;

    const result = luaBridge.execReturn(script);

    /* Check if result is an error (starts with "bad" or contains "fail") */
    if (result && !result.startsWith('[') && !result.startsWith('nil')) {
        return { success: true, data: result };
    }

    /* Check for nil result */
    if (result === '' || result === 'nil' || result === 'false') {
        const err = luaBridge.getError();
        return { success: false, error: err || 'GET failed' };
    }

    return { success: true, data: result };
});

ipcMain.handle('client:set', (event, { invokeId, classId, obis, attrId, data }) => {
    if (!luaBridge) {
        return { success: false, error: 'Bridge not initialized' };
    }

    const obisStr = obis.join('.');
    const dataHex = Array.from(data).map(b => b.toString(16).padStart(2, '0')).join('');

    /* Convert hex string to binary in Lua */
    const script = `
        local hex = "${dataHex}"
        local bin = ""
        for i = 1, #hex, 2 do
            bin = bin .. string.char(tonumber(hex:sub(i, i+1), 16))
        end
        setCosem(${classId}, obis("${obisStr}"), ${attrId}, bin)
    `;

    const error = luaBridge.exec(script);
    if (error) {
        return { success: false, error };
    }

    return { success: true };
});

/* ── Object browser IPC ──────────────────────────────────────────────────── */

ipcMain.handle('cosem:getObjectList', () => {
    if (!luaBridge) {
        return { success: false, error: 'Bridge not initialized' };
    }

    const script = `hex(getObjectList())`;
    const result = luaBridge.execReturn(script);

    if (result && result.length > 0 && !result.startsWith('[')) {
        return { success: true, data: result };
    }

    const err = luaBridge.getError();
    return { success: false, error: err || 'Failed to get object list' };
});

ipcMain.handle('cosem:getClock', () => {
    if (!luaBridge) {
        return { success: false, error: 'Bridge not initialized' };
    }

    const script = `hex(getClock())`;
    const result = luaBridge.execReturn(script);

    if (result && result.length > 0 && !result.startsWith('[')) {
        return { success: true, data: result };
    }

    const err = luaBridge.getError();
    return { success: false, error: err || 'Failed to get clock' };
});

/* ── Dialog ──────────────────────────────────────────────────────────────── */

ipcMain.handle('dialog:open', async () => {
    const result = await dialog.showOpenDialog(mainWindow, {
        properties: ['openFile'],
        filters: [
            { name: 'Lua Scripts', extensions: ['lua'] },
            { name: 'JSON Config', extensions: ['json'] },
            { name: 'All Files', extensions: ['*'] },
        ],
    });
    return result;
});

ipcMain.handle('dialog:save', async (event, { defaultPath, content }) => {
    const result = await dialog.showSaveDialog(mainWindow, {
        defaultPath: defaultPath || 'script.lua',
        filters: [
            { name: 'Lua Scripts', extensions: ['lua'] },
            { name: 'All Files', extensions: ['*'] },
        ],
    });

    if (!result.canceled && result.filePath) {
        fs.writeFileSync(result.filePath, content, 'utf-8');
        return { success: true, path: result.filePath };
    }

    return { success: false };
});

/* ── App lifecycle ───────────────────────────────────────────────────────── */

app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
    if (luaBridge) {
        luaBridge = null;
    }
    if (process.platform !== 'darwin') {
        app.quit();
    }
});

app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
        createWindow();
    }
});
