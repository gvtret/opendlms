/**
 * \file preload.js
 * \brief Preload script for Electron.js context isolation
 */

const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('openDLMS', {
    getVersion: () => ipcRenderer.invoke('get-version'),

    /* Lua bridge */
    execScript: (script) => ipcRenderer.invoke('lua:exec', script),
    execReturn: (script) => ipcRenderer.invoke('lua:execReturn', script),
    execFile: (filename) => ipcRenderer.invoke('lua:execFile', filename),
    getOutput: () => ipcRenderer.invoke('lua:getOutput'),
    clearOutput: () => ipcRenderer.invoke('lua:clearOutput'),
    isConnected: () => ipcRenderer.invoke('lua:isConnected'),
    getError: () => ipcRenderer.invoke('lua:getError'),
    disconnect: () => ipcRenderer.invoke('lua:disconnect'),

    /* Client API (via Lua) */
    clientGet: (opts) => ipcRenderer.invoke('client:get', opts),
    clientSet: (opts) => ipcRenderer.invoke('client:set', opts),

    /* Object browser */
    getObjectList: () => ipcRenderer.invoke('cosem:getObjectList'),
    getClock: () => ipcRenderer.invoke('cosem:getClock'),

    /* Dialogs */
    openFile: () => ipcRenderer.invoke('dialog:open'),
    saveFile: (opts) => ipcRenderer.invoke('dialog:save', opts),
});
