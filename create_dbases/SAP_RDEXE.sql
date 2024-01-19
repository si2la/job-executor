DROP TABLE IF EXISTS Scenarios;
CREATE TABLE Scenarios(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, priority INT, active INT, system INT, workWeekdays TEXT, lastTimestamp INT);
-- INSERT INTO Scenarios VALUES(1, 'Включить тепло', 0, 1); -- наивысший приоритет
INSERT INTO Scenarios VALUES(2, 'Game',  1, 0, 0, '0-1-2-3-4-5-6', 0); -- средний приоритет, неактивный, несистемный, выполняется все дни недели
INSERT INTO Scenarios VALUES(3, 'Alive Log',  1, 1, 1, '0-1-2-3-4-5-6', 0); -- средний приоритет, активный, системный, выполняется все дни недели

DROP TABLE IF EXISTS Actions;
CREATE TABLE Actions(id INTEGER PRIMARY KEY AUTOINCREMENT, scenarioId INTEGER REFERENCES Scenarios(id) ON DELETE CASCADE, label TEXT, nStr INT, actOnce INT, operationId INTEGER);
CREATE INDEX actionsindex ON Actions(scenarioId);  -- Add speed for working

-- INSERT INTO Actions VALUES(1, 1, '', 100, 1, 12);    -- сценарий "включить тепло", нет метки, номер строки 1, выполнить один раз (1), 12 - операция "присвоить значение" или "присвоение"
INSERT INTO Actions VALUES(2, 2, '', 100, 0, 9);    -- сценарий "Game", нет метки, номер строки 100, неоднократное действие (0), 11 - "часы" (RTC)
INSERT INTO Actions VALUES(3, 2, '', 200, 0, 1000);    -- сценарий "Game", нет метки, номер строки 200, неоднократное действие (0), 1000 - "конец сценария"
INSERT INTO Actions VALUES(4, 2, '', 300, 0, 11);    -- сценарий "Game", нет метки, номер строки 300, неоднократное действие (0), 11 - логическая операция "инверсия"
INSERT INTO Actions VALUES(5, 2, '', 400, 0, 17);    -- сценарий "Game", нет метки, номер строки 400, неоднократное действие (0), 17 - "возврат"
INSERT INTO Actions VALUES(6, 2, '', 500, 0, 0);    -- сценарий "Game", нет метки, номер строки 500, неоднократное действие (0), 0 - "нет операции"
INSERT INTO Actions VALUES(7, 3, '', 1, 0, 9);    -- сценарий "Alive Log", нет метки, номер строки 1, неоднократное действие (0), 11 - "часы" (RTC)
INSERT INTO Actions VALUES(8, 3, '', 200, 0, 1000);    -- сценарий "Alive Log", нет метки, номер строки 200, неоднократное действие (0), 1000 - "конец сценария"

DROP TABLE IF EXISTS Parameters;
CREATE TABLE Parameters(id INTEGER PRIMARY KEY AUTOINCREMENT, actionId INTEGER REFERENCES Actions(id) ON DELETE CASCADE, pnumber INT, pvalue TEXT, ptype TEXT, ptag INT, connchannelId INTEGER, pvariableType TEXT, ptimerRepeater TEXT, lastExecTime INT, scenarioId INT);
CREATE INDEX parsindex ON Parameters(actionId);  -- Add speed for working

-- INSERT INTO Parameters VALUES(1, 1, 1, '', 'Actuator', NULL, 3, '', '', 0, 1);    -- действие (Action) id=1, номер параметра 1, значение не исп., "исполн. элемент", тега нет, канал - Теплый пол (COILS 1), ост - нет, сценарий 1
-- INSERT INTO Parameters VALUES(2, 1, 2, 'ON', 'Constant', 1, NULL, '', '', 0, 1);    -- действие id=1, номер параметра 2, имя действия "ON", "константа", тег - значение (1) , канал - NULL, остального - нет, сценарий 1
INSERT INTO Parameters VALUES(3, 2, 1, '', 'RTC', 1548325256, NULL, '', 'EveryMinute', 0, 2);    -- действие id=2, номер параметра 1, значение не исп., "часы", тег- время старта, канал - NULL, тип перем - нет, ежеминутно, сценарий 2
INSERT INTO Parameters VALUES(4, 2, 2, '', 'Call', 300, NULL, '', '', 0, 2);    -- действие id=2, номер параметра 2, значение - нет, "переход (Program Counter)", тег- адрес перехода 300 , канал - NULL, остального - нет, сценарий 2
INSERT INTO Parameters VALUES(5, 3, 1, 'END', 'Control', NULL, NULL, '', '', 0, 2);    -- action_id=3, номер параметра 1, значение "конец сценария", тип - "управление", тип перем нет, канал - NULL, остального - нет, сценарий 2
INSERT INTO Parameters VALUES(6, 4, 1, '', 'Actuator', '', 1, '', '', 0, 2);    -- действие id=4, номер параметра 1, значение не исп., тип - "исполн. элемент", тега нет, канал - лампа на кухне (COILS 5), ост - нет, сценарий 2
INSERT INTO Parameters VALUES(7, 4, 2, '', 'Sensor', '', 2, '', '', 0, 2);    -- действие id=4, номер параметра 2, значение "ON", тип - "Sensor", тип перем нет, канал - лампа на кухне датчик (COILS 5), остального - нет, сценарий 2
INSERT INTO Parameters VALUES(8, 4, 3, '~', 'Logical', '', NULL, 'INVERSION', '', 0, 2);    -- действие id=4, номер параметра 3, значение "ON", тип - "Logical", тип перем нет, канал - лампа на кухне датчик (COILS 5), остального - нет, сценарий 2
INSERT INTO Parameters VALUES(9, 5, 1, 'RETURN', 'Control', NULL, NULL, '', '', 0, 2);    -- action_id=5, номер параметра 1, значение "возврат", тип - "управление", тип перем нет, канал - NULL, остального - нет. Для NOP, END и RETURN параметр это дополнительное описание, сценарий 2
INSERT INTO Parameters VALUES(10, 6, 1, 'NOP', 'Control', NULL, NULL, '', '',0, 2);    -- action_id=5, номер параметра 1, значение "нет операции", тип - "управление", тип перем нет, канал - NULL, остального - нет, сценарий 2
INSERT INTO Parameters VALUES(11, 7, 1, '', 'RTC', 1548325256, NULL, '', 'Hourly', 0, 3);    -- действие id=7, номер параметра 1, значение не исп., "часы", тег- время старта, канал - NULL, тип перем - нет, каждый час, сценарий 3
INSERT INTO Parameters VALUES(12, 7, 2, 'ALIVE', 'Log', NULL, NULL, '', '', 0, 3);    -- действие id=7, номер параметра 2, значение - системный лог "приложение живое", "логирование", тега нет , канал - NULL, остального - нет, сценарий 3
INSERT INTO Parameters VALUES(13, 8, 1, 'END', 'Control', NULL, NULL, '', '', 0, 3);    -- action_id=7, номер параметра 1, значение "конец сценария", тип - "управление", тип перем нет, канал - NULL, остального - нет, сценарий 3

-- переключение состояний для сценария "включить тепло" можно осуществить командами 
-- root@visionsom6ull:~/leo/executor# ./sqlite3 test.db < actuator_off.sql 
-- root@visionsom6ull:~/leo/executor# ./sqlite3 test.db < actuator_on.sql 
-- 23.04.2019 в таблицу Parameters внесено поле scenarioId INT, необходимо только для удобства копирования данных сценария из БД web в БД rdexe
