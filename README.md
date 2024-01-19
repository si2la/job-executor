# JOB EXECUTOR (JEXECUTOR)

## ru
Проект - реализация некоторых задумок на тему: выполнения задач автоматизации по определенному расписанию (сценариям).
Проектировалось как ПО для контроллера Умного дома. Однако, хочется сделать что-то универсальное для задач автоматики.
Поддержка протоколов: Modbus, MQTT.

### Главные задачи: 
- грамотное внедрение однократно исполняемых событий (не сценариев),
- отладка работы по MQTT-протоколу,
- увеличение быстродействия системы,
- выполнение программы на любом оборудовании (в том числе на микрокомпьютерах).

### Установка:
- требуются следующие инструменты:
  - SQLite,
  - gcc, make,
  - redis.

Для построения сыенариев используется БД SQLite, установить ее в вашу систему можно по-разному, однако чтобы скомпилировать приложение, необходимо скачать "объединенный тарбол" - https://www.sqlite.org/download.html#amalgtarball и распаковать его в папку с проектом.

Далее, приложение обменивается информацией с "внешним миром" через in-memory DB Redis. Его тоже необходимо установить в систему вместе с библиотекой для разработки hiredis.

Для дистрибутива Ubuntu/Mint это делается командой:

```
sudo apt install -y redis libhiredis-dev

```
На встраиваемую систему, это можно сделать из исходников, например.

## en
The project is the implementation of some ideas on the topic: performing automation tasks according to a specific schedule (scenarios).
Designed as software for a Smart Home controller to make sheduled tasks. However, I would like to make something universal for automation tasks.
Protocol support: Modbus, MQTT.

### Main tasks:
- competent implementation of one-time executed events (not scenarios),
- debugging work using the MQTT protocol,
- increasing system speed,
- execution of the program on any equipment (including on microcomputers).

### Installation:
- the following tools are required:
   - SQLite,
   - gcc, make,
   - redis.

To build scenaries, the SQLite database is used; you can install it on your system in different ways, but to compile the application, you need to download the "amalgamation tarball" - https://www.sqlite.org/download.html#amalgtarball and unpack it into a folder with project.

Next, the application exchanges information with the "outside world" through in-memory DB Redis. It also needs to be installed on the system along with the hiredis development library.

For the Ubuntu/Mint distribution, this is done with the command:

```
sudo apt install -y redis libhiredis-dev

```
For an embedded system, this can be done from source code, for example.


