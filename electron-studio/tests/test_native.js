/**
 * \file test_native.js
 * \brief Tests for OpenDLMS native addon
 *
 *  Run: node tests/test_native.js
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

'use strict';

const assert = require('assert');
const path = require('path');

let native;
try {
    native = require(path.join(__dirname, '..', 'native', 'build', 'Release', 'opendlms-native.node'));
} catch (e) {
    console.error('Failed to load native addon:', e.message);
    console.error('Build first: cd electron-studio && npm run build:native');
    process.exit(1);
}

let passed = 0;
let failed = 0;

function test(name, fn) {
    try {
        fn();
        passed++;
        console.log(`  PASS: ${name}`);
    } catch (e) {
        failed++;
        console.log(`  FAIL: ${name}`);
        console.log(`        ${e.message}`);
    }
}

console.log('OpenDLMS Native Addon Tests\n');

/* ── Version ─────────────────────────────────────────────────────────────── */

console.log('Version:');

test('VERSION is defined', () => {
    assert.strictEqual(typeof native.VERSION, 'string');
    assert.strictEqual(native.VERSION, '1.1.0');
});

test('Framing constants defined', () => {
    assert.strictEqual(native.FRAMING_NONE, 0);
    assert.strictEqual(native.FRAMING_WRAPPER, 1);
    assert.strictEqual(native.FRAMING_HDLC, 2);
});

/* ── Transport ───────────────────────────────────────────────────────────── */

console.log('\nTransport:');

test('Transport can be instantiated', () => {
    const t = new native.Transport('127.0.0.1', 4056);
    assert.ok(t);
    assert.strictEqual(t.isConnected(), false);
});

test('Transport connect fails to unreachable host', () => {
    const t = new native.Transport('192.0.2.1', 1); /* TEST-NET, should timeout */
    assert.strictEqual(t.isConnected(), false);
});

/* ── Client ──────────────────────────────────────────────────────────────── */

console.log('\nClient:');

test('Client can be instantiated', () => {
    const c = new native.Client();
    assert.ok(c);
});

/* ── Server ──────────────────────────────────────────────────────────────── */

console.log('\nServer:');

test('Server can be instantiated', () => {
    const s = new native.Server();
    assert.ok(s);
});

/* ── LuaBridge ───────────────────────────────────────────────────────────── */

console.log('\nLuaBridge:');

test('LuaBridge can be instantiated', () => {
    const lb = new native.LuaBridge();
    assert.ok(lb);
    assert.strictEqual(lb.isConnected(), false);
});

test('LuaBridge execReturn: arithmetic expression', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('2 + 2');
    assert.strictEqual(result, '4');
});

test('LuaBridge execReturn: string expression', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('"hello" .. " world"');
    assert.strictEqual(result, 'hello world');
});

test('LuaBridge execReturn: boolean expression', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('true');
    assert.strictEqual(result, 'true');
});

test('LuaBridge execReturn: nil expression', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('nil');
    assert.strictEqual(result, '');
});

test('LuaBridge execReturn: hex function', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('hex("\\x01\\x02\\x03")');
    assert.strictEqual(result, '01 02 03');
});

test('LuaBridge execReturn: obis function', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('type(obis("0.0.1.0.0.255"))');
    assert.strictEqual(result, 'table');
});

test('LuaBridge execReturn: print captures output', () => {
    const lb = new native.LuaBridge();
    lb.execReturn('print("test output")');
    const output = lb.getOutput();
    assert.ok(output.includes('test output'));
});

test('LuaBridge execReturn: clear output', () => {
    const lb = new native.LuaBridge();
    lb.execReturn('print("to clear")');
    lb.clearOutput();
    const output = lb.getOutput();
    assert.strictEqual(output, '');
});

test('LuaBridge execReturn: error on invalid syntax', () => {
    const lb = new native.LuaBridge();
    lb.execReturn('invalid syntax !!!');
    const err = lb.getError();
    assert.ok(err.length > 0);
});

test('LuaBridge execReturn: multi-line script', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn(`
        local a = 10
        local b = 20
        return a + b
    `);
    assert.strictEqual(result, '30');
});

test('LuaBridge execReturn: function call', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('string.upper("hello")');
    assert.strictEqual(result, 'HELLO');
});

test('LuaBridge execReturn: table serialization', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('string.format("%d.%d.%d", 1, 0, 255)');
    assert.strictEqual(result, '1.0.255');
});

/* ── Summary ─────────────────────────────────────────────────────────────── */

console.log(`\n${passed + failed} tests, ${passed} passed, ${failed} failed`);
process.exit(failed > 0 ? 1 : 0);
