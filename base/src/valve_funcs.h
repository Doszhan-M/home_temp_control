extern HTTPClient restClient;
extern WiFiClient client;


String valveState = "UNKNOWN";
bool refreshValveState = false;

// Отправить запрос на выключение реле
int close_valve()
{
  const char *closeUrl = "http://192.168.4.2/close_valve/";                           // url relay
  restClient.begin(client, closeUrl);
  int ResponseStatusCode = restClient.GET();                                          // get запрос
  if (ResponseStatusCode == 200)
  {
    Serial.print("Relay has been switched off! Status: ");
    Serial.println(ResponseStatusCode);
    Serial.println("Valve has been closed");
    refreshValveState = true;
    valveState = "CLOSED";
    String payload = restClient.getString();
    Serial.println(payload);
  }
  else
  {
    Serial.print("Error! StatusCode code: ");
    Serial.println(ResponseStatusCode);
  }
  return ResponseStatusCode;
};

// Отправить запрос на включение реле
int open_valve()
{
  const char *closeUrl = "http://192.168.4.2/open_valve/";                              // url relay
  restClient.begin(client, closeUrl);
  int ResponseStatusCode = restClient.GET();                                            // get запрос
  if (ResponseStatusCode == 200)
  {
    Serial.print("Relay has been switched on! Status: ");
    Serial.println(ResponseStatusCode);
    Serial.println("Valve has been opened");
    refreshValveState = true;
    valveState = "OPEN";
  }
  else
  {
    Serial.print("Error! HTTP Response code: ");
    Serial.println(ResponseStatusCode);
  }
  return ResponseStatusCode;
};

// Отправить запрос на закрытие клапана во время старта
void close_valve_startup()
{
  int ResponseStatusCode = close_valve();                               
  if (ResponseStatusCode != 200)
  {
    Serial.println("Recursion");
    return close_valve_startup();
  }
};