# WashingMachineAlert
Receive notification on my phone when my washing machine is done

Washing machine model: `SAMSUNG WD80T4046EW/EF`

It uses an [Arduino microphone](https://fr.aliexpress.com/wholesale?trafficChannel=main&d=y&CatId=0&SearchText=microphone+arduino&ltype=wholesale&SortType=default&page=2) and an ESP32 espressif dev board.
Sampling the sound and getting the frequency representation (with FastFourierTransform) we can detect specific frequency and send the appropriate message on a Discord channel to get a notification on my phone.

It can either sends a Discord notification or a home assistant one.

