#Cliente FTP Concurrente en C (RFC 959)

El proyecto implementa un **cliente FTP concurrente** desarrollado en C, siguiendo las especificaciones del **RFC 959**.  
Es capaz de manejar **transferencias simultáneas** de archivos utilizando **procesos hijos (`fork()`)**, manteniendo activa la conexión de control.

El cliente implementa los comandos básicos del protocolo FTP y algunos adicionales.

---

#Funcionalidades Implementadas

##Comandos básicos (obligatorios – RFC 959)
| Comando | Descripción |
|--------|-------------|
| `USER` | Envía el nombre de usuario al servidor FTP. |
| `PASS` | Envía la contraseña asociada. |
| `PASV` | Activa modo pasivo: el servidor brinda un puerto de datos. |
| `PORT` | Activa modo activo: el cliente escucha y el servidor se conecta. |
| `RETR` | Descarga un archivo del servidor (concurrente). |
| `STOR` | Sube un archivo al servidor (concurrente). |

---

##Comandos adicionales implementados
| Comando | Descripción |
|--------|-------------|
| `MKD`  | Crea un directorio remoto. |
| `PWD`  | Muestra el directorio actual del servidor. |
| `DELE` | Elimina un archivo en el servidor. |
| `REST` | Reinicia transferencias a partir de un offset (reanudar). |

---

## Concurrencia
Cada transferencia (**RETR** o **STOR**) se ejecuta en un proceso hijo:

- Permite descargar/subir múltiples archivos al mismo tiempo.
- La conexión de control permanece activa.
- El usuario puede seguir ejecutando comandos mientras se transfieren archivos.

---

# Arquitectura del Proyecto

El cliente utiliza los siguientes archivos:

- `FloresJ-clienteFTP.c` — Lógica principal del cliente y parser de comandos.
- `connectsock.c` — Función auxiliar de conexión a sockets.
- `connectTCP.c` — Abstracción para conexiones TCP a un host:puerto.
- `errexit.c` — Función de error estándar.

---

#Compilación

Este proyecto incluye un **Makefile**.  
Para compilar, simplemente ejecutar:

```bash
make
