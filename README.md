# Ramsey 250QM

250cc motosikletin elektrik tesisatının ESP32 tabanlı [Wokwi](https://wokwi.com) simülasyonu.

## Wokwi'de açma

1. https://wokwi.com/projects/new/esp32 adresinden yeni bir ESP32 projesi açın.
2. `wokwi/sketch.ino` içeriğini `sketch.ino` sekmesine yapıştırın.
3. `diagram.json` sekmesine `wokwi/diagram.json` içeriğini yapıştırın.
4. Dosya menüsünden **New file → `libraries.txt`** oluşturup `wokwi/libraries.txt` içeriğini yapıştırın.
5. ▶ ile simülasyonu başlatın.

## Kullanım

| Adım | Ne yapılır |
|------|------------|
| Kontak | **Kontak** anahtarını açın: gösterge paneli ve arka lamba yanar |
| Hazırlık | **Kill** anahtarı RUN konumunda, **Boş vites** açık olmalı |
| Marş | **Marş** butonunu ~1,2 sn basılı tutun: röle çeker, motor çalışır, far otomatik yanar |
| Sürüş | **Boş vites**i kapatın, **Gaz kolu** ile devir/hız değişir |
| Fren | **Fren** butonu stop lambasını tam parlaklıkta yakar ve yavaşlatır |
| Sinyal | **Sol/Sağ sinyal** aç/kapat, **Dörtlü flaşör** ikisini birden yakar |
| Far | **Uzun far** anahtarı kısa/uzun far arasında geçiş yapar |
| Korna | **Korna** butonu basılıyken öter |

Yakıt ve sıcaklık potansiyometreleriyle düşük yakıt (<%15) ve aşırı sıcaklık (>105 °C) uyarılarını deneyebilirsiniz.

## Pin tablosu (ESP32 DevKit V1)

| GPIO | Yön | Bağlantı |
|------|-----|----------|
| 18 | Çıkış (PWM) | Kısa far |
| 19 | Çıkış | Uzun far |
| 23 | Çıkış (PWM) | Arka lamba / stop |
| 4 | Çıkış | Sol ön + sol arka sinyal |
| 5 | Çıkış | Sağ ön + sağ arka sinyal |
| 15 | Çıkış | Korna (buzzer) |
| 2 | Çıkış | Marş rölesi |
| 13 | Giriş (pull-up) | Kontak anahtarı |
| 14 | Giriş (pull-up) | Kill anahtarı (LOW = RUN) |
| 27 | Giriş (pull-up) | Marş butonu |
| 26 | Giriş (pull-up) | Korna butonu |
| 25 | Giriş (pull-up) | Sol sinyal butonu |
| 33 | Giriş (pull-up) | Sağ sinyal butonu |
| 16 | Giriş (pull-up) | Dörtlü flaşör butonu |
| 17 | Giriş (pull-up) | Uzun far anahtarı |
| 36 (VP) | Giriş, harici 10k pull-up | Fren müşiri |
| 39 (VN) | Giriş, harici 10k pull-up | Boş vites müşiri |
| 34 | Analog | Gaz kolu (TPS) |
| 35 | Analog | Yakıt şamandırası |
| 32 | Analog | Motor sıcaklığı |
| 21 / 22 | I2C | SSD1306 OLED gösterge (SDA / SCL) |

Tüm anahtar ve butonlar basıldığında/açıldığında GPIO'yu GND'ye çeker.

## Gerçek motora uyarlarken

Simülasyondaki LED'ler ve buzzer yalnızca temsilidir. Gerçek 12 V tesisatta ESP32 pinleri
lambaları doğrudan süremez: her yük için MOSFET (ör. IRLZ44N) veya röle, 12 V → 5 V/3,3 V
regülatör, girişlerde gerilim bölücü/optokuplör ve sigorta kullanılmalıdır.
