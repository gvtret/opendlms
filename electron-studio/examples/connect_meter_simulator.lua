local host = "127.0.0.1"
local port = tonumber(os.getenv("OPENDLMS_METER_PORT") or "4063")

print(string.format("Connecting to meter simulator at %s:%d", host, port))

local ok, err = connect(host, port)
if not ok then
    error(err or "connection failed")
end

print("Connected")

print("Reading Association LN object list")

local data, err = getObjectList()
if not data then
    print("Object list GET failed: " .. tostring(err))
    return
end

print("Object list response bytes: " .. tostring(#data))
print(hex(data))
