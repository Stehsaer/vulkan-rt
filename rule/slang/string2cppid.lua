local alphabet = "etaoinsrhdlucmfy"

function encode(input)
  if input == nil then
    return nil
  end

  local out = {}
  local insert = table.insert

  for i = 1, #input do
    local byte = input:byte(i) & 0xFF
    local c1 = ((byte & 0x40) >> 3) | ((byte & 0x10) >> 2) | ((byte & 0x04) >> 1) | (byte & 0x01)
    local c2 = ((byte & 0x80) >> 4) | ((byte & 0x20) >> 3) | ((byte & 0x08) >> 2) | ((byte & 0x02) >> 1)

    insert(out, alphabet:sub(c1 + 1, c1 + 1))
    insert(out, alphabet:sub(c2 + 1, c2 + 1))
  end

  return table.concat(out)
end