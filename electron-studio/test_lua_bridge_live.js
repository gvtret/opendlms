const { app } = require('electron');
const path = require('path');

app.whenReady().then(() => {
  const native = require(path.join(__dirname, 'native', 'build', 'Release', 'opendlms-native.node'));
  const bridge = new native.LuaBridge();
  const port = Number(process.argv[2] || process.env.OPENDLMS_METER_PORT || 4063);
  const script = `
local ok, err = connect("127.0.0.1", ${port})
if not ok then error(err or "connect failed") end
local data, err = getObjectList()
if not data then error(err or "getObjectList failed") end
if #data == 0 then error("getObjectList returned empty response") end
print("object_list_len=" .. tostring(#data))
disconnect()
`;
  const err = bridge.exec(script);
  const out = bridge.getOutput();
  if (out) {
    process.stdout.write(out);
  }
  if (err) {
    console.error(err);
    app.exit(1);
  } else {
    app.exit(0);
  }
});
