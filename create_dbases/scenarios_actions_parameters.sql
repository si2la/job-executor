DROP TABLE IF EXISTS Scenarios;
CREATE TABLE Scenarios(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, priority INT, active INT, system INT, workWeekdays TEXT, lastTimestamp INT);
-- INSERT INTO Scenarios VALUES(1, 'Включить тепло', 0, 1); -- наивысший приоритет
INSERT INTO Scenarios VALUES(2, 'Game',  1, 0, 0, '0-1-2-3-4-5-6', 0); -- средний приоритет, неактивный, несистемный, выполняется все дни недели
INSERT INTO Scenarios VALUES(3, 'Alive Log',  1, 1, 1, '0-1-2-3-4-5-6', 0); -- средний приоритет, активный, системный, выполняется все дни недели

DROP TABLE IF EXISTS Actions;
CREATE TABLE Actions(id INTEGER PRIMARY KEY AUTOINCREMENT, scenarioId INTEGER REFERENCES Scenarios(id) ON DELETE CASCADE, label TEXT, nStr INT, actOnce INT, operationId INTEGER REFERENCES Operations(id) ON DELETE SET
 NULL ON UPDATE CASCADE);
CREATE INDEX actionsindex ON Actions(scenarioId);  -- Add speed for working
CREATE INDEX actionsOnOperationIndex ON Actions(operationId);

-- INSERT INTO Actions VALUES(1, 1, '', 100, 1, 12);    -- сценарий "включить тепло", нет метки, номер строки 1, выполнить один раз (1), 12 - операция "присвоить значение" или "присвоение"
INSERT INTO Actions VALUES(2, 2, '', 100, 0, 9);    -- сценарий "Game", нет метки, номер строки 100, неоднократное действие (0), 11 - "часы"
INSERT INTO Actions VALUES(3, 2, '', 200, 0, 1000);    -- сценарий "Game", нет метки, номер строки 200, неоднократное действие (0), 1000 - "конец сценария"
INSERT INTO Actions VALUES(4, 2, '', 300, 0, 11);    -- сценарий "Game", нет метки, номер строки 300, неоднократное действие (0), 11 - логическая операция "инверсия"
INSERT INTO Actions VALUES(5, 2, '', 400, 0, 17);    -- сценарий "Game", нет метки, номер строки 400, неоднократное действие (0), 17 - "возврат"
INSERT INTO Actions VALUES(6, 2, '', 500, 0, 0);    -- сценарий "Game", нет метки, номер строки 500, неоднократное действие (0), 0 - "нет операции"
INSERT INTO Actions VALUES(7, 3, '', 1, 0, 9);    -- сценарий "Alive Log", нет метки, номер строки 1, неоднократное действие (0), 11 - "часы"
INSERT INTO Actions VALUES(8, 3, '', 200, 0, 1000);    -- сценарий "Alive Log", нет метки, номер строки 200, неоднократное действие (0), 1000 - "конец сценария"

DROP TABLE IF EXISTS Parameters;
CREATE TABLE Parameters(id INTEGER PRIMARY KEY AUTOINCREMENT, actionId INTEGER REFERENCES Actions(id) ON DELETE CASCADE, pnumber INT, pvalue TEXT, ptype TEXT, ptag INT, connchannelId INTEGER REFERENCES Connchannels(id) ON DELETE SET
 NULL ON UPDATE CASCADE, pvariableType TEXT, ptimerRepeater TEXT, lastExecTime INT, scenarioId INT);
CREATE INDEX parsindex ON Parameters(actionId);  -- Add speed for working
CREATE INDEX parsOnConnchannelsIndex ON Parameters(connchannelId); 
-- первые параметры - оставлены для истории
-- 31.03.2020 поле type в действиях RETURN & END оставлено "Control" в остальных - тип  в type операции
-- INSERT INTO Parameters VALUES(1, 1, 1, '', 'Actuator', NULL, 3, '', '', 0, 1);    -- действие (Action) id=1, номер параметра 1, значение не исп., "исполн. элемент", тега нет, канал - Теплый пол (COILS 1), ост - нет, сценарий 1
-- INSERT INTO Parameters VALUES(2, 1, 2, 'ON', 'Constant', 1, NULL, '', '', 0, 1);    -- действие id=1, номер параметра 2, имя действия "ON", "константа", тег - значение (1) , канал - NULL, остального - нет, сценарий 1
INSERT INTO Parameters VALUES(3, 2, 1, '', 'RTC', 1548325256, NULL, '', 'EveryMinute', 0, 2);    -- действие actionId=2, номер параметра 1, значение pvalue не исп., "часы", тег- время старта, канал - NULL, тип перем - нет, ежеминутно, lastExecTime=0, scenarioId=2
INSERT INTO Parameters VALUES(4, 2, 2, '', 'Call', 300, NULL, '', '', 0, 2);    -- действие actionId=2, номер параметра 2, значение pvalue - нет, "переход (Program Counter)", тег- адрес перехода 300 , канал - NULL, остального - нет, сценарий scenarioId=2
INSERT INTO Parameters VALUES(5, 3, 1, 'END', 'Control', NULL, NULL, '', '', 0, 2);    -- action_id=3, номер параметра 1, значение "конец сценария", тип - "управление", тип перем нет, канал - NULL, остального - нет, сценарий 2
INSERT INTO Parameters VALUES(6, 4, 1, '~', 'BITS', '', NULL, '', '', 0, 2);
INSERT INTO Parameters VALUES(7, 4, 2, '', 'Actuator', '', 1, '', '', 0, 2);
INSERT INTO Parameters VALUES(8, 4, 3, '', 'Sensor', '', 2, '', '', 0, 2);
INSERT INTO Parameters VALUES(9, 4, 4, '', '', '', NULL, '', '', 0, 2);
INSERT INTO Parameters VALUES(10, 5, 1, 'RETURN', 'Control', NULL, NULL, '', '', 0, 2);
INSERT INTO Parameters VALUES(11, 6, 1, 'NOP', 'Control', NULL, NULL, '', '',0, 2);
INSERT INTO Parameters VALUES(12, 7, 1, '', 'RTC', 1548325256, NULL, '', 'Hourly', 0, 3);
INSERT INTO Parameters VALUES(13, 7, 2, 'ALIVE', 'Log', NULL, NULL, '', '', 0, 3);
INSERT INTO Parameters VALUES(14, 8, 1, 'END', 'Control', NULL, NULL, '', '', 0, 3);

-- переключение состояний для сценария "включить тепло" осуществляется командами - c этого все начиналось :) 
-- root@visionsom6ull:~/leo/executor# ./sqlite3 test.db < actuator_off.sql 
-- root@visionsom6ull:~/leo/executor# ./sqlite3 test.db < actuator_on.sql 

-- поле connchannelId INTEGER REFERENCES Connchannels(id) в таблице параметров необходимо для сложного запроса при редактировании сценария, запрос нужно переделать, соответственно нужно ли поле?? - пока нужно!
-- 23.04.2019 в таблицу Parameters внесено поле scenarioId INT, необходимо только для удобства копирования данных сценария из БД web в БД rdexe
-- 28.01.2020 в таблицу Scenarios внесено поле  system INT индикатор системных событий (не отображаются на странице events) и workWeekdays TEXT для обработки сценария по дням недели, а также lastTimestamp - на будущее (время последнего исполнения) для концепции событий
-- ALTER TABLE Scenarios ADD system INT;
-- ALTER TABLE Scenarios ADD workWeekdays TEXT;
-- ALTER TABLE Scenarios ADD lastTimestamp INT;
-- добавить информацию во всех записях:
-- UPDATE Scenarios SET workWeekdays='0-1-2-3-4-5-6'






