extern HTTPClient restClient;
extern WiFiClient client;

using thermo::Valve;

// Связь с модулем реле (relay/) по HTTP через свою точку доступа.
// Команда повторяется, пока реле её не подтвердит, а подтверждённое состояние
// отправляется заново раз в минуту: так реле после перезагрузки получит актуальную
// команду, а его сторожевой таймер знает, что контроллер жив.

const char *RELAY_OPEN_URL  = "http://192.168.4.2/open_valve/";
const char *RELAY_CLOSE_URL = "http://192.168.4.2/close_valve/";
const uint16_t RELAY_HTTP_TIMEOUT_MS = 1500;     // чтобы энкодер не замирал надолго
const uint32_t RELAY_RETRY_MS        = 30000;    // повтор неподтверждённой команды
const uint32_t RELAY_HEARTBEAT_MS    = 60000;    // повтор подтверждённой команды

Valve valveState = Valve::Unknown;   // последнее состояние, подтверждённое реле
bool relayLink = true;               // false — реле не ответило на последнюю команду
uint32_t relayLastAttempt = 0;
bool relayAttempted = false;

const char *valveName(Valve v) {
  switch (v) {
    case Valve::Open:   return "OPEN";
    case Valve::Closed: return "CLOSED";
    default:            return "UNKNOWN";
  }
}

// Отправить команду реле. true — реле ответило 200.
bool sendValveCommand(Valve v) {
  if (WiFi.softAPgetStationNum() == 0) {
    Serial.println(F("Relay: not connected to AP"));
    return false;   // реле не подключено к точке доступа — не ждём таймаут
  }
  restClient.setTimeout(RELAY_HTTP_TIMEOUT_MS);
  if (!restClient.begin(client, v == Valve::Open ? RELAY_OPEN_URL : RELAY_CLOSE_URL)) {
    return false;
  }
  int code = restClient.GET();
  restClient.end();
  Serial.printf("Relay: %s -> %d\n", valveName(v), code);
  return code == 200;
}

// Довести реле до нужного состояния. Вызывается на каждом проходе loop.
// Возвращает true, если изменилось что-то, что видно на экране.
bool syncRelay(Valve desired, uint32_t nowMs) {
  if (desired == Valve::Unknown) return false;
  bool confirmed = (valveState == desired);
  uint32_t wait = confirmed ? RELAY_HEARTBEAT_MS : RELAY_RETRY_MS;
  // новая (ещё не отправленная) команда уходит сразу
  bool newCommand = !confirmed && relayLink;
  if (relayAttempted && !newCommand && !thermo::intervalPassed(nowMs, relayLastAttempt, wait)) {
    return false;
  }
  relayAttempted = true;
  relayLastAttempt = nowMs;

  Valve prevState = valveState;
  bool prevLink = relayLink;
  if (sendValveCommand(desired)) {
    valveState = desired;
    relayLink = true;
  } else {
    relayLink = false;
    valveState = Valve::Unknown;   // реле не подтвердило — состояние неизвестно
  }
  return prevState != valveState || prevLink != relayLink;
}
