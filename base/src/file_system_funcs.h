#include "LittleFS.h"


// Считать настройки из пзу
String readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if (!file || file.isDirectory())
  {
    Serial.println("- empty file or failed to open file");
    return String();
  }
  String fileContent;
  while (file.available())
  {
    fileContent += String((char)file.read());
  }
  return fileContent;
};

// Считать число из файла. Если файла нет или значение вне [lo, hi] — вернуть def.
float readFloatSetting(fs::FS &fs, const char *path, float def, float lo, float hi)
{
  String value = readFile(fs, path);
  value.trim();
  if (value.length() == 0) return def;
  float v = value.toFloat();
  if (v < lo || v > hi) {
    Serial.printf("- value %s out of range, using default\r\n", value.c_str());
    return def;
  }
  return v;
}

// Записать настройки в пзу память ------------------------------------------
void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if (!file)
  {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("- file written");
  }
  else
  {
    Serial.println("- write failed");
  }
};
