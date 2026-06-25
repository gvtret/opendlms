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

    /* Read file and execute */
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
        return { success: false, error: 'Not connected' };
    }

    /* Execute via Lua bridge */
    const script = `
        local data, err = getCosem(${classId}, obis(${JSON.stringify(obis.join('.'))}), ${attrId})
        if data then
            return hex(data)
        else
            error(err or "GET failed")
        end
    `;
    /* TODO: implement direct API without Lua */
    return { success: false, error: 'Use Lua script for now' });
});

ipcMain.handle('client:set', (event, { invokeId, classId, obis, attrId, data }) => {
    if (!luaBridge) {
        return { success: false, error: 'Not connected' };
    }

    return { success: false, error: 'Use Lua script for now' });
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
