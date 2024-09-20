Implementare:
    Flowul programului incepe in main, unde se verifica cateva conditii
    generale pentru fiecare pachet primit de catre router (lungimea pachetului
    sa fie cel putin cat structura de ethernet, tipul pachetului sa fie de tip
    IPv4 sau ARP). Mai departe, incepem manipularea pachetelor.
    In cazul in care pachetul este de tip ARP, verificam tipul de operatie,
    fie pachetul este de tip ARP Request si trebuie sa raspund cu adresa MAC
    routerului, fie este de tipul reply la un pachet trimis de router si trebuie
    trimis mai pachetul a carui destinatie este sursa de unde a venit pachetul
    de reply. Pentru cazul in care pachetul este de tip IPv4, verificam campul
    de checksum daca este corect. Verificam daca campul time-to-live este inca
    valid, iar pentru cazul in care pachetul a expirat, trimitem un pachet de
    tip ICMP cu tipul si codul corespunzator catre sursa pentru a informa ca a
    expirat pachetul. Pentru cazul cand avem un ICMP de tipul Echo Request,
    iar routerul este destinatia (verificam prin IP-uri), trimitem un Echo Reply.
    Altfel, trimitem pachetul curent spre urmatoarea destinatie. 

Puncte rezolvate:
    1. Procesul de dirijare : Aici doar verificam tipul pachetului sa fie
                              IPv4 si integritatea pachetului prin checksum.
                              In procesul de trimitere mai departe, folosim
                              tabela de routare pentru gasirea interfetei de
                              forward. Schimbam MAC-ul destinatie si sursa,
                              time-to-live, recalculam checksum-ul si
                              trimitem pachetul pe interfata urmatoare.

    2. Longest Prefix Match : Pentru tabela de routare am folosit trie, unde
                              fiecare nod reprezinta un bit dintr-o adresa,
                              fie 0 fie 1 (astfel fiecare nod are doi maxim doi
                              copii). Cand citesc tabela, parcurg fiecare bit din
                              adresa de prefix, iar la fiecare pas dau shift la
                              masca pana cand devine 0 (se termina match-ul).
                              In continutul nodului de final (cand se termina
                              adresa IP de match) retinem o variabila prin care
                              semnalam sfarsitul unui match si interfata cu
                              nexthopul corespunzator. Pentru search la un IP,
                              parcurgem trie-ul in functie de adresa noastra in jos
                              pana la ultimul match si avem garantat ca fiind cel
                              mai lung prefix.

    3. Protocolul ARP : Avem doua tipuri de pachete, fie cand primim un request catre
                        router si trebuie sa trimitem ca reply adresa noastra MAC.
                        Iar daca avem un ARP reply de la o sursa, verificam in coada
                        noastra de pachete daca avem un pachet care sa aiba ca destinatie
                        adresa IP a pachetului ARP de unde provine. Adaugam perechea IP-MAC
                        in lista de arp cache, si trimitem pachetul care era pe waiting.

    4. Protocolul ICMP : Pentru ICMP avem urmatoare cazuri : fie trimite un
                         ICMP Echo Reply, ICMP Timeout si ICMP Destination Unreachable.
                         Pentr Echo Reply, verificam tipul si codul si faptul ca noi suntem
                         destinatia, schimbam adresele MAC si IP si trimitem pachetul inapoi
                         de unde a venit.
                         Pentru ICMP Timeout, verificam daca campul time-to-live, iar daca
                         pachetul a expirat, creez un pachet nou cu date corespunzatoare
                         si trimit la adresa IP de unde provine pachetul.
                         Asemantor si pentru ICMP Destination Unreachable, facem un nou pachet
                         cu datele corespunzatoare si trimiem inapoi pachetul.
