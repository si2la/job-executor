-- таблица для сбора статистики по подключенному каналу (ConnchannelsId)
DROP TABLE IF EXISTS Statistics;
CREATE TABLE Statistics(id INTEGER PRIMARY KEY AUTOINCREMENT, ccid INT, timestamp INT, value TEXT);
INSERT INTO Statistics VALUES(0, 4, 1, '0');

