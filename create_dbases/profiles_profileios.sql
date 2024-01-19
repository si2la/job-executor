DROP TABLE IF EXISTS Profiles;
CREATE TABLE Profiles(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, fullName TEXT, protos TEXT, speeds TEXT, uartParams TEXT, active INT);
INSERT INTO Profiles VALUES(1, 'DRM88ER', '8-канальное Интернет реле, ООО "Разумный дом"', 'MODBUS_TCP#MODBUS_RTU', '9600#19200#38400#57600#115200#230400', '8-N-2', 1);
INSERT INTO Profiles VALUES(2, 'DRM88RL', '8-канальный релейный модуль, ООО "Разумный дом"', 'MODBUS_RTU', '9600#19200#38400#57600#115200', '8-N-2', 1);
INSERT INTO Profiles VALUES(3, 'DRM88Rv2', '8-канальный релейный модуль, ООО "Разумный дом"', 'MODBUS_RTU', '9600#19200#38400#57600#115200#230400', '8-N-2', 1);
INSERT INTO Profiles VALUES(4, 'DDL44EM-U', '4-канальный диммер регулятор 0-10В, ШИМ, ООО "Разумный дом"', 'MODBUS_TCP#MODBUS_RTU', '9600#19200#38400#57600#115200#230400', '8-N-2', 1);
INSERT INTO Profiles VALUES(5, 'DDL44EM-I', '4-канальный диммер регулятор 4-20мА, ШИМ, ООО "Разумный дом"', 'MODBUS_TCP#MODBUS_RTU', '9600#19200#38400#57600#115200#230400', '8-N-2', 1);
INSERT INTO Profiles VALUES(90, 'MSU44ER', '4-канальный датчик с аналоговыми сенсорами, ООО "Разумный дом"', 'MODBUS_TCP#MODBUS_RTU', '9600#19200#38400#57600#115200', '8-N-2', 1);
INSERT INTO Profiles VALUES(91, 'MSU44R', 'Модуль измерительный, ООО "Разумный дом"', 'MODBUS_RTU', '9600#19200#38400#57600#115200', '8-N-2', 1);
INSERT INTO Profiles VALUES(92, 'PLC018R', '8-канальный индикаторный модуль, ООО "Разумный дом"', 'MODBUS_RTU', '9600#19200#38400#57600#115200', '8-N-2', 1);

INSERT INTO Profiles VALUES(100, 'MQTT client', 'Клиент MQTT сервиса', 'MQTT', '', '', 1);
INSERT INTO Profiles VALUES(101, 'HIDEN Modulator', 'Модулятор для осветительного оборудования фирмы HIDEN', 'MODBUS_RTU', '9600', '8-N-1', 1);

DROP TABLE IF EXISTS Profileios;
CREATE TABLE Profileios(id INTEGER PRIMARY KEY AUTOINCREMENT, proto TEXT, typeio TEXT, funct TEXT, count INT, active INT, ProfileId INT);
INSERT INTO Profileios VALUES(1,    'MODBUS_RTU', 0, 'COILS', 8, 1, 1);
INSERT INTO Profileios VALUES(2,    'MODBUS_RTU', 1, 'COILS', 8, 1, 1);
INSERT INTO Profileios VALUES(3,    'MODBUS_TCP', 0, 'COILS', 8, 1, 1);
INSERT INTO Profileios VALUES(4,    'MODBUS_TCP', 1, 'COILS', 8, 1, 1);
INSERT INTO Profileios VALUES(5,    'MODBUS_RTU', 0, 'DI', 8, 1, 1);
INSERT INTO Profileios VALUES(6,    'MODBUS_TCP', 0, 'DI', 8, 1, 1);
INSERT INTO Profileios VALUES(7,    'MODBUS_RTU', 0, 'IR', 68, 1, 1);
INSERT INTO Profileios VALUES(8,    'MODBUS_TCP', 0, 'IR', 68, 1, 1);
INSERT INTO Profileios VALUES(9,    'MODBUS_RTU', 0, 'HR', 5060, 1, 1);
INSERT INTO Profileios VALUES(10,   'MODBUS_RTU', 1, 'HR', 5060, 1, 1);
INSERT INTO Profileios VALUES(11,   'MODBUS_TCP', 0, 'HR', 5060, 1, 1);
INSERT INTO Profileios VALUES(12,   'MODBUS_TCP', 1, 'HR', 5060, 1, 1);

INSERT INTO Profileios VALUES(13,   'MODBUS_RTU', 0, 'COILS', 8, 1, 2);
INSERT INTO Profileios VALUES(14,   'MODBUS_RTU', 1, 'COILS', 8, 1, 2);
INSERT INTO Profileios VALUES(15,   'MODBUS_RTU', 0, 'DI', 8, 1, 2);

INSERT INTO Profileios VALUES(16,   'MODBUS_RTU', 0, 'COILS', 22, 1, 3);
INSERT INTO Profileios VALUES(17,   'MODBUS_RTU', 1, 'COILS', 22, 1, 3);
INSERT INTO Profileios VALUES(18,   'MODBUS_RTU', 0, 'DI', 8, 1, 3);
INSERT INTO Profileios VALUES(19,   'MODBUS_RTU', 0, 'IR', 1022, 1, 3);
INSERT INTO Profileios VALUES(20,   'MODBUS_RTU', 0, 'HR', 2658, 1, 3);
INSERT INTO Profileios VALUES(21,   'MODBUS_RTU', 1, 'HR', 2658, 1, 3);

INSERT INTO Profileios VALUES(22,   'MODBUS_RTU', 0, 'COILS', 4, 1, 4);
INSERT INTO Profileios VALUES(23,   'MODBUS_RTU', 1, 'COILS', 4, 1, 4);
INSERT INTO Profileios VALUES(24,   'MODBUS_TCP', 0, 'COILS', 4, 1, 4);
INSERT INTO Profileios VALUES(25,   'MODBUS_TCP', 1, 'COILS', 4, 1, 4);
INSERT INTO Profileios VALUES(26,   'MODBUS_RTU', 0, 'IR', 50, 1, 4);
INSERT INTO Profileios VALUES(27,   'MODBUS_TCP', 0, 'IR', 50, 1, 4);
INSERT INTO Profileios VALUES(28,   'MODBUS_RTU', 0, 'HR', 5060, 1, 4);
INSERT INTO Profileios VALUES(29,   'MODBUS_RTU', 1, 'HR', 5060, 1, 4);
INSERT INTO Profileios VALUES(30,   'MODBUS_TCP', 0, 'HR', 5060, 1, 4);
INSERT INTO Profileios VALUES(31,   'MODBUS_TCP', 1, 'HR', 5060, 1, 4);

INSERT INTO Profileios VALUES(32,   'MODBUS_RTU', 0, 'COILS', 4, 1, 5);
INSERT INTO Profileios VALUES(33,   'MODBUS_RTU', 1, 'COILS', 4, 1, 5);
INSERT INTO Profileios VALUES(34,   'MODBUS_TCP', 0, 'COILS', 4, 1, 5);
INSERT INTO Profileios VALUES(35,   'MODBUS_TCP', 1, 'COILS', 4, 1, 5);
INSERT INTO Profileios VALUES(36,   'MODBUS_RTU', 0, 'IR', 50, 1, 5);
INSERT INTO Profileios VALUES(37,   'MODBUS_TCP', 0, 'IR', 50, 1, 5);
INSERT INTO Profileios VALUES(38,   'MODBUS_RTU', 0, 'HR', 5060, 1, 5);
INSERT INTO Profileios VALUES(39,   'MODBUS_RTU', 1, 'HR', 5060, 1, 5);
INSERT INTO Profileios VALUES(40,   'MODBUS_TCP', 0, 'HR', 5060, 1, 5);
INSERT INTO Profileios VALUES(41,   'MODBUS_TCP', 1, 'HR', 5060, 1, 5);

INSERT INTO Profileios VALUES(500,  'MODBUS_RTU', 0, 'COILS', 4, 1, 90);
INSERT INTO Profileios VALUES(501,  'MODBUS_RTU', 1, 'COILS', 4, 1, 90);
INSERT INTO Profileios VALUES(502,  'MODBUS_TCP', 0, 'COILS', 4, 1, 90);
INSERT INTO Profileios VALUES(503,  'MODBUS_TCP', 1, 'COILS', 4, 1, 90);
INSERT INTO Profileios VALUES(504,  'MODBUS_RTU', 0, 'IR', 150, 1, 90);
INSERT INTO Profileios VALUES(505,  'MODBUS_TCP', 0, 'IR', 150, 1, 90);
INSERT INTO Profileios VALUES(506,  'MODBUS_RTU', 0, 'HR', 5060, 1, 90);
INSERT INTO Profileios VALUES(507,  'MODBUS_RTU', 1, 'HR', 5060, 1, 90);
INSERT INTO Profileios VALUES(508,  'MODBUS_TCP', 0, 'HR', 5060, 1, 90);
INSERT INTO Profileios VALUES(509,  'MODBUS_TCP', 1, 'HR', 5060, 1, 90);

INSERT INTO Profileios VALUES(510,  'MODBUS_RTU', 0, 'COILS', 4, 1, 91);
INSERT INTO Profileios VALUES(511,  'MODBUS_RTU', 1, 'COILS', 4, 1, 91);
INSERT INTO Profileios VALUES(512,  'MODBUS_RTU', 0, 'DI', 8, 1, 91);
INSERT INTO Profileios VALUES(513,  'MODBUS_RTU', 0, 'IR', 48, 1, 91);
INSERT INTO Profileios VALUES(514,  'MODBUS_RTU', 0, 'HR', 1682, 1, 91);
INSERT INTO Profileios VALUES(515,  'MODBUS_RTU', 1, 'HR', 1682, 1, 91);

INSERT INTO Profileios VALUES(516,  'MODBUS_RTU', 0, 'COILS', 18, 1, 92);
INSERT INTO Profileios VALUES(517,  'MODBUS_RTU', 1, 'COILS', 18, 1, 92);
INSERT INTO Profileios VALUES(518,  'MODBUS_RTU', 0, 'HR', 62, 1, 92);
INSERT INTO Profileios VALUES(519,  'MODBUS_RTU', 1, 'HR', 62, 1, 92);

INSERT INTO Profileios VALUES(1000, 'MQTT', 0, 'SUB', 100, 1, 100);
INSERT INTO Profileios VALUES(1001, 'MQTT', 1, 'PUB', 100, 1, 100);
INSERT INTO Profileios VALUES(1002, 'MODBUS_RTU', 0, 'HR', 1, 1, 101);  -- слово состояния Модулятора HIDEN
INSERT INTO Profileios VALUES(1003, 'MODBUS_RTU', 1, 'HR', 6, 1, 101);   -- 6 регистров управления (запись) Write Single Register HIDEN

