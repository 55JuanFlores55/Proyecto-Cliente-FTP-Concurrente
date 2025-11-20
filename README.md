# Proyecto-Cliente-FTP-Concurrente
El objetivo del proyecto es desarrollar un cliente FTP concurrente con las siguientes especificaciones funcionales:

El programa debe utilizar las funciones connectsock.c connectTCP.c errexit.c
El programa debe implementar los commandos básicos del protocolo FTP, especificados en el RFC 959 (https://datatracker.ietf.org/doc/html/rfc959): USER, PASS, STOR, RETR, PORT, PASV. Si desean obtener extra crédito, pueden implementar commandos adicionales: MKD, PWD y DELE, REST, etc. 
El programa debe tener la capacidad de transferir varios archivos (en los dos sentidos) y mantener activa la conexión de control, de manera concurrente. 
Pueden utilizar procesos para la implementación de concurrencia.
