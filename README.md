# Tema 2 Protocoale de Comunicatie

### Balaniuc Rodion 325CB

## Descriere

Acest proiect implementeaza un sistem de publicare-subscriere:
- Server: gestioneaza conexiuni TCP cu clientii si primeste mesaje UDP de la publicanti
- Clienti: se conecteaza prin TCP si se aboneaza la topicuri
- Publicanti: trimit mesaje UDP catre server pentru redistribuire

## Structura proiect

- `server.cpp`: Implementarea serverului TCP/UDP
- `subscriber.cpp`: Implementarea clientului TCP
- `utils.h`: Fucntii utilitare comune

### Comenzi client-server
- `subscribe <topic>`: abonare la topic
- `unsubscribe <topic>`: dezabonare de la topic

Format mesaj:
1. Primii 2 octeti: lungimea mesajului (network byte order)
2. Continutul mesajului

### Mesaje UDP
Format:
1. Primii 50 octeti: numele topicului
2. Urmatorul octet: tipul datelor (0-3)
3. Restul octetilor: continutul mesajului

Tipuri de date:
- `INT_TYPE (0)`: Numar intreg cu semn
- `SHORT_REAL_TYPE (1)`: Numar real cu 2 zecimale
- `FLOAT_TYPE (2)`: Numar real cu 4 zecimale
- `STRING_TYPE (3)`: Sir de caractere

### Mesaje server-client
Format:
- `<topic> - INT - <valoare>`
- `<topic> - SHORT_REAL - <valoare>`
- `<topic> - FLOAT - <valoare>`
- `<topic> - STRING - <valoare>`

## Implementare server

### Structuri de date
```cpp
struct TcpClient {
    int sockfd;
    std::string id;
    std::set<std::string> subscriptions;
};

std::unordered_map<std::string, TcpClient> clients;
std::unordered_map<int, std::string> fd_to_id;
```

### Functionalitati principale
1. **Gestionare conexiuni**: Foloseste `select()` pentru a gestiona simultan:
   - Conexiuni TCP noi
   - Comenzi de la clienti
   - Mesaje UDP
   - Comenzi de la stdin

2. **Potrivire topicuri**: Suporta:
   - Potrivire exacta: `sensor/temperature`
   - Wildcard pentru un nivel: `sensor/+`
   - Wildcard pentru mai multe niveluri: `sensor/*`

3. **Procesare mesaje UDP**: Extrage si formateaza datele din mesajele UDP

4. **Transmitere catre abonati**: Trimite mesaje catre clientii abonati

## Implementare client

1. **Conectare**: Se conecteaza la server si trimite ID-ul
2. **Gestionare I/O**: Foloseste `select()` pentru comenzi si mesaje
3. **Trimitere comenzi**: Trimite comenzi de abonare/dezabonare
4. **Afisare mesaje**: Primeste si afiseaza mesajele formatate

## Detalii tehnice

### Gestionare conexiuni multiple
Serverul foloseste `select()` pentru a gestiona eficient multiple conexiuni fara a folosi thread-uri.

### Algoritm de potrivire topicuri
Implementat recursiv in `match()` din `utils.h`:
- `+` se potriveste cu exact un nivel
- `*` se potriveste cu zero sau mai multe niveluri

### Transfer de date
Functiile `send_all()` si `recv_all()` asigura transmiterea completa a datelor.

### Manipulare formate
Datele sunt transferate in network byte order folosind `htons()`, `ntohs()`, `htonl()` si `ntohl()`.

### Comenzi client
- `subscribe <topic>`: abonare la topic
- `unsubscribe <topic>`: dezabonare de la topic
- `exit`: inchidere client

## Limitari
- ID client: maxim 10 caractere
- Nume topic: maxim 50 caractere
- Server: maxim 1000 de clienti
- Clientii deconectati raman in memorie pentru pastrarea abonamentelor
