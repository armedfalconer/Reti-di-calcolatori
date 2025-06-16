Es. con i sensori.

# A
Si ha un software che simula un sensore (dati vari). I sensori devono poter fare:
1. pubblicizzarsi con un server centrale in maniera affidabile (io esisto) (TCP)
2. Ogni tick inviare dati sensoristici in maniera non affidabile (UDP)

# B
Dato questo programma, inventare una condizione per cui situazione d'allarme per la quale anche il sensore può ricevere ed entrare in stato di allarme. Se in stato di allarme, il sensore non invierà più i dati (punto 2).
Il sensore deve verificare i dati.

# C
Il server centrale in maniera affidabile e sicura deve comunicare al sensore dopo 5 secondi di allarme di riattivarsi (riavviarsi se si riesce).