DROP TABLE IF EXISTS Operations;
-- Таблица Operations: возможные операции для действий сценария
CREATE TABLE Operations(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, nOperands INT, fullName TEXT);
INSERT INTO Operations VALUES(0,  'NOP', 1, 'Пустое действие');
--INSERT INTO Operations VALUES(1,  'LINEAR', 6, 'Формула');
--INSERT INTO Operations VALUES(2,  'THRESHOLD', 7, 'Пороговое реле');
--INSERT INTO Operations VALUES(3,  'THRESHOLD_INVERTED', 7, 'Пороговое инверс рел');
--INSERT INTO Operations VALUES(4,  'TRIGGER ', 6, 'Триггер');
--INSERT INTO Operations VALUES(5,  'DIRECT', 6, 'Выход = входу');
--INSERT INTO Operations VALUES(6,  'TIMER', 7, 'Таймер');
--INSERT INTO Operations VALUES(7,  'JAL', 6, 'Жалюзи');
--INSERT INTO Operations VALUES(8,  'PID', 7, 'ПИД регулятор');
-- 26.02.2020 RTC было 4 операнда - стало ?
INSERT INTO Operations VALUES(9,  'RTC', 4, 'Часы и дата');
INSERT INTO Operations VALUES(10, 'MATH', 7, 'Математические операции');
INSERT INTO Operations VALUES(11, 'BITS', 4, 'Битовые операции');
INSERT INTO Operations VALUES(12, 'SET', 2, 'Установить значение');
INSERT INTO Operations VALUES(13, 'LOGIC(IF)', 4, 'Условный оператор');
INSERT INTO Operations VALUES(14, 'GOTO', 1, 'Переход');
INSERT INTO Operations VALUES(15, 'RGB2A', 4, 'RGB->Actuators');
--INSERT INTO Operations VALUES(16, 'A2RGB', 4, 'Actuators->RGB');
INSERT INTO Operations VALUES(17, 'RETURN', 1, 'Возврат из подпрограммы');
INSERT INTO Operations VALUES(1000, 'END', 1, 'Конец');

DROP TABLE IF EXISTS Operands;
-- nOperands в таблице Operations это количество записей в таблице Parameters, относящихся к Actions (operationId)
-- Таблица Operands: перечислимые типы для операндов и nOperands не связаны!!! Это количество и варианты изменяемых через веб-интерфейс параметров (/home/rd/leo/executor/create_dbases/operations_operands.sql надо бы подправить - сменить на nParameters!!!)
CREATE TABLE Operands(id INTEGER PRIMARY KEY AUTOINCREMENT, OperationId INTEGER REFERENCES Operations(id), number INT, typeList TEXT, defaultValue TEXT, fullName TEXT);
CREATE INDEX operandsOnOperationsIndex ON Operands(operationId);
-- ALG_LINEAR
-- INSERT INTO Operands VALUES(1, 1, 1, 'Variable#Actuator#Timer', 'Variable', 'Result');
-- INSERT INTO Operands VALUES(2, 1, 2, '(K / N) * X + B#(K / N) / X + B#(K / N) * (X + B)', '(K / N) * X + B', '=');
-- INSERT INTO Operands VALUES(3, 1, 3, 'Variable#Actuator#Sensor#Timer', 'Variable', 'X');
-- INSERT INTO Operands VALUES(4, 1, 4, 'Constant#Variable', 'Variable', 'K');
-- INSERT INTO Operands VALUES(5, 1, 5, 'Constant#Variable', 'Variable', 'N');
-- INSERT INTO Operands VALUES(6, 1, 6, 'Constant#Variable', 'Variable', 'B');
-- ALG_SET
INSERT INTO Operands VALUES(7, 12, 1, 'Variable#Actuator#Statistic#Timer', 'Variable', 'SET Result (Operand 1');
INSERT INTO Operands VALUES(8, 12, 2, 'Constant#Sensor#Variable', 'Constant', 'SET Operand 2');
-- ALG_END
INSERT INTO Operands VALUES(9, 1000, 1, '', '', 'END');
-- ALG_NOP
INSERT INTO Operands VALUES(10, 0, 1, '', '', 'NOP');
-- ALG_RTC
INSERT INTO Operands VALUES(11, 9, 1, 'Once#Every5Seconds#Every10Seconds#EveryMinute#Hourly#Daily#Weekly#Monthly', 'Once', 'RTC Repeater Variants');
-- #26.02.2020 remove Set from this list, but in scenario server save handler yet
INSERT INTO Operands VALUES(12, 9, 2, 'Goto#Call#Log', 'Call', 'RTC operation choice');
INSERT INTO Operands VALUES(13, 9, 3, 'PC#Label', 'PC', 'RTC/PC Operand 1');
INSERT INTO Operands VALUES(14, 9, 4, 'LogString', 'LogString', 'RTC/Log Operand 1');
INSERT INTO Operands VALUES(15, 9, 5, 'Variable#Actuator#Statistic', 'Variable', 'RTC/SET Operand 1');
INSERT INTO Operands VALUES(16, 9, 6, 'Constant#Sensor', 'Constant', 'RTC/SET Operand 2');
-- ALG_LOGIC
INSERT INTO Operands VALUES(17, 13, 1, 'Variable#Sensor#Timer', 'Variable', 'IF left condition part');
INSERT INTO Operands VALUES(18, 13, 2, '>#>=#<#<=#==#!=', '>', 'Sign');
INSERT INTO Operands VALUES(19, 13, 3, 'Constant#Sensor', 'Constant', 'IF right condition part');
INSERT INTO Operands VALUES(20, 13, 4, 'Goto#Call#Log', 'Call', 'IF operation choice');
INSERT INTO Operands VALUES(21, 13, 5, 'PC#Label', 'PC', 'IF/PC Operand 1');
INSERT INTO Operands VALUES(22, 13, 6, 'LogString', 'LogString', 'IF/Log Operand 1');
--INSERT INTO Operands VALUES(23, 13, 7, 'Variable#Actuator', 'Variable', 'RTC/SET Operand 1');
--INSERT INTO Operands VALUES(24, 13, 8, 'Constant#Sensor', 'Constant', 'RTC/SET Operand 2');
-- ALG_BITS
-- идея битовых операций: переменной или актуатору присвоить результат логической операции
-- инверсия - удобно мигать лампой (опрокинуть значение)
-- & и | - включить чтото если еще что-то включено или выключено
INSERT INTO Operands VALUES(25, 11, 1, 'Variable#Actuator', 'Variable', 'Result');
INSERT INTO Operands VALUES(26, 11, 2, 'Variable#Sensor', 'Sensor', 'Left operand');
INSERT INTO Operands VALUES(27, 11, 3, '~#&#|', '~', 'operation');
INSERT INTO Operands VALUES(28, 11, 4, 'Constant#Sensor', 'Constant', 'Right operand');
-- TODO а где RETURN в параметрах??? Оставим пока 29, 30 запись!!
INSERT INTO Operands VALUES(31, 15, 1, 'Variable', 'Variable', 'RGB value');
INSERT INTO Operands VALUES(32, 15, 2, 'Actuator', 'Actuator', 'R actuator');
INSERT INTO Operands VALUES(33, 15, 3, 'Actuator', 'Actuator', 'G actuator');
INSERT INTO Operands VALUES(34, 15, 4, 'Actuator', 'Actuator', 'B actuator');
--INSERT INTO Operands VALUES(35, 16, 1, 'Variable', 'Variable', 'RGB value');
--INSERT INTO Operands VALUES(36, 16, 2, 'Actuator', 'Actuator', 'R actuator');
--INSERT INTO Operands VALUES(37, 16, 3, 'Actuator', 'Actuator', 'G actuator');
--INSERT INTO Operands VALUES(38, 16, 4, 'Actuator', 'Actuator', 'B actuator');
-- MATH (математические операции) +-*/ и % - остаток от деления
-- Вид А = В + С на примере сложения
INSERT INTO Operands VALUES(40, 10, 1, 'Variable#Actuator', 'Actuator', 'Result (A)');
-- идея: иногда к значению датчика нужно что-то прибавить/отнять, ну и просто... чтобы было...
INSERT INTO Operands VALUES(41, 10, 2, 'Variable#Sensor', 'Variable', 'B operand');
INSERT INTO Operands VALUES(42, 10, 3, '+#-#*#/#%', '+', 'Operation type');
INSERT INTO Operands VALUES(43, 10, 4, 'Variable#Constant', 'Variable', 'C operand');








