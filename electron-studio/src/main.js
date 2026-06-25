/**
 * \file main.js
 * \brief Electron.js main process for OpenDLMS Studio
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

const { app, BrowserWindow, ipcMain, dialog } = require('electron');
const path = require('path');

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
}

/* ── IPC Handlers ────────────────────────────────────────────────────────── */

ipcMain.handle('get-version', () => {
    return native.VERSION;
});

ipcMain.handle('transport:create', (event, host, port) => {
    try {
        const transport = new native.Transport(host, port || 4056);
        return { success: true, id: transportId++ };
    } catch (e) {
        return { success: false, error: e.message };
    }
});

ipcMain.handle('client:create', () => {
    try {
        const client = new native.Client();
        return { success: true };
    } catch (e) {
        return { success: false, error: e.message };
    }
});

ipcMain.handle('client:get', (event, { invokeId, classId, obis, attrId }) => {
    /* TODO: track client instances */
    return { success: false, error: 'Not implemented' };
});

ipcMain.handle('client:getBlock', (event, { invokeId, classId, obis, attrId }) => {
    return { success: false, error: 'Not implemented' };
});

ipcMain.handle('client:set', (event, { invokeId, classId, obis, attrId, data }) => {
    return { success: false, error: 'Not implemented' };
});

ipcMain.handle('client:setBlock', (event, { invokeId, classId, obis, attrId, data }) => {
    return { success: false, error: 'Not implemented' };
});

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
    if (process.platform !== 'darwin') {
        app.quit();
    }
});

app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
        createWindow();
    }
});
