/**
 * \file preload.js
 * \brief Preload script for Electron.js context isolation
 */

const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('openDLMS', {
    getVersion: () => ipcRenderer.invoke('get-version'),

    /* Transport */
    createTransport: (host, port) => ipcRenderer.invoke('transport:create', host, port),

    /* Client */
    createClient: () => ipcRenderer.invoke('client:create'),
    clientGet: (opts) => ipcRenderer.invoke('client:get', opts),
    clientGetBlock: (opts) => ipcRenderer.invoke('client:getBlock', opts),
    clientSet: (opts) => ipcRenderer.invoke('client:set', opts),
    clientSetBlock: (opts) => ipcRenderer.invoke('client:setBlock', opts),

    /* Dialogs */
    openFile: () => ipcRenderer.invoke('dialog:open'),
});
