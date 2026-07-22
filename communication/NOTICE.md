# Communication module sources / licenses

- Qt 6 Network / SerialPort / SerialBus: commercial or LGPL/GPL per Qt license chosen by the project.
- Modbus framing and register/coil packing implemented in-tree from the public
  [Modbus Application Protocol Specification V1.1b](https://www.modbus.org/file/secure/modbusprotocolspecification.pdf).
- No GPL/AGPL third-party Modbus libraries were vendored into this repository.
- In-process MockTransport / MockModbusMap are original Selt code for offline tests.
