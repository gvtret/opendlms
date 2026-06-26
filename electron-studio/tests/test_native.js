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
    native = require(path.join(__dirname, '..', 'native', 'build', 'opendlms-native.node'));
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

/* ── LuaBridge ───────────────────────────────────────────────────────────── */

console.log('\nLuaBridge:');

test('LuaBridge can be instantiated', () => {
    const lb = new native.LuaBridge();
    assert.ok(lb);
    assert.strictEqual(lb.isConnected(), false);
});

test('execReturn: arithmetic expression', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('2 + 2');
    assert.strictEqual(result, '4');
});

test('execReturn: string expression', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('"hello" .. " world"');
    assert.strictEqual(result, 'hello world');
});

test('execReturn: boolean expression', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('true');
    assert.strictEqual(result, 'true');
});

test('execReturn: nil expression', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('nil');
    assert.strictEqual(result, '');
});

test('execReturn: hex function', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('hex("\\x01\\x02\\x03")');
    assert.strictEqual(result, '01 02 03');
});

test('execReturn: obis function', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('type(obis("0.0.1.0.0.255"))');
    assert.strictEqual(result, 'table');
});

test('execReturn: print captures output', () => {
    const lb = new native.LuaBridge();
    lb.execReturn('print("test output")');
    const output = lb.getOutput();
    assert.ok(output.includes('test output'));
});

test('execReturn: clear output', () => {
    const lb = new native.LuaBridge();
    lb.execReturn('print("to clear")');
    lb.clearOutput();
    const output = lb.getOutput();
    assert.strictEqual(output, '');
});

test('execReturn: error on invalid syntax', () => {
    const lb = new native.LuaBridge();
    lb.execReturn('invalid syntax !!!');
    const err = lb.getError();
    assert.ok(err.length > 0);
});

test('execReturn: multi-line script', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('(function() local a = 10; local b = 20; return a + b end)()');
    assert.strictEqual(result, '30');
});

test('execReturn: function call', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('string.upper("hello")');
    assert.strictEqual(result, 'HELLO');
});

test('execReturn: table serialization', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('string.format("%d.%d.%d", 1, 0, 255)');
    assert.strictEqual(result, '1.0.255');
});

test('execReturn: math operations', () => {
    const lb = new native.LuaBridge();
    assert.ok(lb.execReturn('10 / 3').startsWith('3.3'));
    assert.strictEqual(lb.execReturn('2 ^ 10'), '1024');
});

test('execReturn: string operations', () => {
    const lb = new native.LuaBridge();
    assert.strictEqual(lb.execReturn('#"hello"'), '5');
    assert.strictEqual(lb.execReturn('string.rep("ab", 3)'), 'ababab');
});

test('execReturn: multiple print lines', () => {
    const lb = new native.LuaBridge();
    lb.clearOutput();
    lb.execReturn('print("line1"); print("line2"); print("line3")');
    const output = lb.getOutput();
    const lines = output.split('\n').filter(l => l.trim());
    assert.strictEqual(lines.length, 3);
});

test('execReturn: boolean operations', () => {
    const lb = new native.LuaBridge();
    assert.strictEqual(lb.execReturn('10 > 5'), 'true');
    assert.strictEqual(lb.execReturn('10 < 5'), 'false');
});

/* ── Summary ─────────────────────────────────────────────────────────────── */

console.log(`\n${passed + failed} tests, ${passed} passed, ${failed} failed`);
process.exit(failed > 0 ? 1 : 0);
