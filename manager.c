#include "util.h"

// Struct de almacenamiento de usuarios
typedef struct {
    char client_pipe[256]; // Descriptor de archivo del pipe para comunicación con el cliente
    char username[USERNAME_LEN]; // Nombre de usuario del cliente
    pid_t pid; // PID del proceso del cliente
} Client;

// Struct de comunicación con el cliente
typedef struct {
    char client_pipe[256]; // Descriptor de archivo del pipe para comunicación con el cliente
    int command_type; // Tipo de comando
    char topic[50];  // Campo para almacenar el nombre del tópico
    char username[50]; // Nombre de usuario del cliente
    pid_t pid; // PID del proceso del cliente
    int lifetime; // Lifetime restante
    char message[TAM_MSG]; // Mensaje que se envía
} Response;

// Struct para la gestión de topicos
typedef struct {
    char name[TOPIC_NAME_LEN]; // Nombre del tópico
    char subscribers[MAX_SUBSCRIBERS][USERNAME_LEN]; // Matriz para almacenar los nombres de usuarios suscritos a un tópico
    int subscriber_count; // Número de suscriptores al tópico.
    int is_locked; // Indicador de si el tópico está bloqueado.
    int has_active_messages;  // Indicador de si el tópico tiene mensajes activos
} Topic;

// Struct para el almacenamiento de mensajes en el archivo
typedef struct {
    char topic[TOPIC_NAME_LEN]; // Nombre del tópico al que pertenece el mensaje
    char username[USERNAME_LEN]; // Nombre del usuario que envió el mensaje
    char message[TAM_MSG];  // El contenido del mensaje
    int lifetime; // Lifetime restante
    Response msg;
} StoredMessage;

// Creación de los hilos
pthread_t lifetime_thread;
pthread_t command_thread;

Topic topics[MAX_TOPICS]; // Almacena los topicos creados
Client clients[MAX_USERS]; // Almacena los usuarios conectados
StoredMessage messages[MAX_MESSAGES]; // Almacena los mensajes de los topicos
int topic_count = 0;
int client_count = 0;
int message_count = 0;
pthread_mutex_t mutex; // Declaración del mutex

// Flag para la eliminación de hilos
int terminate_thread = 0;

// Función para enviar un mensaje a un cliente
void send_response(const char *client_pipe, const char *message) {
    int fd = open(client_pipe, O_WRONLY);
    if (fd != -1) {
        write(fd, message, strlen(message) + 1); // +1 para incluir el carácter nulo
        close(fd);
    } else {
        perror("Error al abrir la pipe del cliente");
    }
}

// Función para eliminar todos los usuarios conectados y cerrar el manager (close y CTRL+C del manager)
void close_all_connections() {
    // Cerrar todas las conexiones de clientes
    for (int i = 0; i < client_count; i++) {
        if (clients[i].pid > 0) {
            kill(clients[i].pid, SIGTERM); // Enviar SIGTERM al cliente
            printf("Se envió SIGTERM a %s (PID: %d)\n", clients[i].username, clients[i].pid);
        }
    }

    // Enviar SIGUSR1 a los hilos
    pthread_kill(lifetime_thread, SIGUSR1);  // Solicitar a lifetime_thread que se cierre
    pthread_kill(command_thread, SIGUSR1);   // Solicitar a command_thread que se cierre

    // Esperar a que los hilos terminen correctamente
    pthread_join(lifetime_thread, NULL);
    printf("Lifetime thread finalizado.\n");

    pthread_join(command_thread, NULL);
    printf("Command thread finalizado.\n");
}


// Función para manejar la señal SIGINT (CTRL+C) del programa
void handle_sigint(int sig) {
    printf("\nServidor finalizado. Limpiando recursos...\n");
    close_all_connections();
    unlink(SERVER_PIPE);
    exit(0); 
}


// Función para añadir un usuario a la lista de usuarios conectados
void add_client(const char *client_pipe, const char *username, pid_t pid) {
    // Verificar si el cliente ya está conectado
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].username, username) == 0) {
            printf("El cliente %s ya está conectado (PID: %d)\n", username, clients[i].pid);
            return; // No agregar el cliente nuevamente
        }
    }

    // Si no está, añadir el cliente
    if (client_count < MAX_USERS) {
        strncpy(clients[client_count].client_pipe, client_pipe, sizeof(clients[client_count].client_pipe) - 1);
        strncpy(clients[client_count].username, username, USERNAME_LEN);
        clients[client_count].pid = pid;
        client_count++;
        printf("Cliente agregado: %s (PID: %d)\n", username, pid);
    } else {
        printf("No se puede agregar el cliente %s. Límite máximo de usuarios alcanzado.\n", username);
    }
}

// Función para suscribir un usuario a un topico y recibir los mensajes de ese topico
void subscribe_topic(const char *topic_name, const char *client_pipe, const char *username) {
    if (strlen(topic_name) >= TOPIC_NAME_LEN) {
        send_response(client_pipe, "Error: El nombre del tópico excede el máximo de caracteres.");
        return;
    }

    // Comprobar si se ha alcanzado el límite de tópicos
    if (topic_count >= MAX_TOPICS) {
        send_response(client_pipe, "Error: máximo de tópicos alcanzado.");
        return;
    }

    // Buscar si el tópico ya existe
    int topic_index = -1;
    for (int i = 0; i < topic_count; i++) {
        if (strcmp(topics[i].name, topic_name) == 0) {
            topic_index = i;
            break;
        }
    }

    // Si no existe el topico, crear uno nuevo y agregar al primer suscriptor
    if (topic_index == -1) {
        strncpy(topics[topic_count].name, topic_name, TOPIC_NAME_LEN);
        topics[topic_count].name[TOPIC_NAME_LEN - 1] = '\0';
        topics[topic_count].subscriber_count = 0;

        // Agregar el primer suscriptor (el usuario que se suscribe)
        strncpy(topics[topic_count].subscribers[0], username, USERNAME_LEN);
        topics[topic_count].subscriber_count++;

        // Imprimir mensaje en el servidor
        printf("El usuario '%s' ha creado y se ha suscrito al tópico '%s'.\n", username, topic_name);

        topic_count++;

        // Enviar respuesta al cliente
        send_response(client_pipe, "Tópico creado y suscrito.");

    }
    else{
        // Buscar el tópico en la lista de tópicos
        for (int i = 0; i < topic_count; i++) {
            if (strcmp(topics[i].name, topic_name) == 0) {
                // Verificar si el usuario ya está suscrito
                for (int j = 0; j < topics[i].subscriber_count; j++) {
                    if (strcmp(topics[i].subscribers[j], username) == 0) {
                        send_response(client_pipe, "Ya estás suscrito al tópico.");
                        return;
                    }
                }

                // Si el usuario no está suscrito, agregarlo
                if (topics[i].subscriber_count < MAX_SUBSCRIBERS) {
                    strncpy(topics[i].subscribers[topics[i].subscriber_count], username, USERNAME_LEN);
                    topics[i].subscribers[topics[i].subscriber_count][USERNAME_LEN - 1] = '\0';
                    topics[i].subscriber_count++;

                    // Imprimir mensaje en el servidor
                    printf("El usuario '%s' se ha suscrito al tópico '%s'.\n", username, topic_name);

                    // Almacenar los mensajes en una lista (buffer)
                    char all_messages[1024 * MAX_MESSAGES];  // Suponiendo un límite de mensajes
                    for (int j = 0; j < message_count; j++) {
                        if (strcmp(messages[j].topic, topic_name) == 0 && messages[j].lifetime > 0) {
                            // Concatenar el mensaje al buffer
                            char message_to_send[1024];
                            snprintf(message_to_send, sizeof(message_to_send), "%s %s %s\n", messages[j].topic, messages[j].username, messages[j].message);
                            strncat(all_messages, message_to_send, sizeof(all_messages) - strlen(all_messages) - 1);
                        }
                    }

                    // Enviar todos los mensajes de una vez
                    if (strlen(all_messages) > 0) {
                        send_response(client_pipe, all_messages);
                    }

                    // Informar a los suscriptores actuales del tópico
                    printf("Usuarios suscritos al tópico '%s':\n", topic_name);
                    for (int j = 0; j < topics[i].subscriber_count; j++) {
                        printf(" - %s\n", topics[i].subscribers[j]);
                    }

                    send_response(client_pipe, "Te has suscrito al tópico.");
                } else {
                    send_response(client_pipe, "Error: máximo de suscriptores alcanzado.");
                }
                return;
            }
        } 
    }
}

// Función para desuscribir un usuario de un topico
void unsubscribe_topic(const char *topic_name, const char *client_pipe, const char *username) {
    // Recorre todos los tópicos para encontrar el tópico al que el usuario desea desuscribirse
    for (int i = 0; i < topic_count; i++) {
        // Verifica si el nombre del tópico coincide con el tópico que el usuario quiere desuscribirse
        if (strcmp(topics[i].name, topic_name) == 0) {
            
            // Recorre los suscriptores del tópico
            for (int j = 0; j < topics[i].subscriber_count; j++) {
                
                // Verifica si el usuario está suscrito a este tópico
                if (strcmp(topics[i].subscribers[j], username) == 0) {
                    
                    // Si el usuario está suscrito, lo elimina de la lista de suscriptores del tópico
                    // Empezamos desde el índice del suscriptor y recorre los suscriptores detrás de él
                    for (int k = j; k < topics[i].subscriber_count - 1; k++) {
                        // Desplaza los suscriptores restantes una posición hacia atrás para sobrescribir al usuario eliminado
                        // Se copia el suscriptor siguiente en la posición donde estaba el suscriptor eliminado y hace lo mismo con los demás
                        strncpy(topics[i].subscribers[k], topics[i].subscribers[k + 1], USERNAME_LEN);
                    }
                    
                    // Disminuye el contador de suscriptores para reflejar la eliminación
                    topics[i].subscriber_count--;
                    
                    // Envia una respuesta al cliente confirmando que se desuscribió correctamente
                    send_response(client_pipe, "Te has desuscrito del tópico.");
                    return;
                }
            }

            // Si el usuario no estaba suscrito al tópico, envía una respuesta al cliente
            send_response(client_pipe, "No estás suscrito al tópico.");
            return;
        }
    }

    // Si no se encuentra el tópico en la lista, se envía una respuesta indicando que el tópico no existe
    send_response(client_pipe, "El tópico no existe.");
}


// Función para listar los topicos
void list_topics(const char *client_pipe) {
    char response[1024] = "Tópicos:\n";

    if (topic_count == 0) {
        // Concatenar "No hay tópicos para listar." a response
        strcat(response, "No hay tópicos para listar.\n");
        printf("No hay tópicos para listar.\n");
    } else {
        // Construir la lista de tópicos
        for (int i = 0; i < topic_count; i++) {
            char topic_info[100];
            snprintf(topic_info, sizeof(topic_info), "- %s (Suscriptores: %d)\n", topics[i].name, topics[i].subscriber_count);
            strcat(response, topic_info);
        }
        printf("Se listaron %d tópicos.\n", topic_count);
    }

    // Enviar la respuesta completa usando response
    send_response(client_pipe, response);
}


// Función para verificar si un tópico existe
int topic_exists(const char *topic_name) {  
    for (int i = 0; i < topic_count; i++) {
        if (strcmp(topics[i].name, topic_name) == 0) {
            return 1;
        }
    }
    return 0;
}

// Función para listar los usuarios conectados
void list_connected_users() {
    if (client_count == 0) {
        printf("No hay usuarios conectados.\n");
        return;
    }

    for (int i = 0; i < client_count; i++) {
        printf("- %s (Pipe: %s)\n", clients[i].username, clients[i].client_pipe);
    }
}

// Función para enviar un mensaje a un topico
void send_message(Response* request) {
    // Verificar si el tópico existe
    int topic_index = -1;
    for (int i = 0; i < topic_count; i++) {
        if (strcmp(topics[i].name, request->topic) == 0) {
            topic_index = i;
            break;
        }
    }

    // Si el tópico no existe, crearlo
    if (topic_index == -1) {
        if (topic_count < MAX_TOPICS) {
            // Inicializar el nuevo tópico
            strncpy(topics[topic_count].name, request->topic, TOPIC_NAME_LEN);
            topics[topic_count].name[TOPIC_NAME_LEN - 1] = '\0';
            topics[topic_count].subscriber_count = 0; // Sin suscriptores iniciales
            topics[topic_count].is_locked = 0;       // No bloqueado por defecto
            topics[topic_count].has_active_messages = 0; // Sin mensajes activos inicialmente

            topic_index = topic_count;
            topic_count++;

            printf("Tópico '%s' creado automáticamente.\n", request->topic);
        } else {
            send_response(request->client_pipe, "Error: No se pueden crear más tópicos, límite alcanzado.");
            return;
        }
    }

    // Verificar si el tópico está bloqueado
    if (topics[topic_index].is_locked) {
        send_response(request->client_pipe, "El tópico está bloqueado. No se puede enviar el mensaje.");
        return;
    }


    // Si el mensaje es persistente, verificar el número de mensajes persistentes en el tópico
    if (request->lifetime > 0) {
        int persistent_message_count = 0;

        // Contar los mensajes persistentes en el tópico
        for (int i = 0; i < message_count; i++) {
            if (strcmp(messages[i].topic, request->topic) == 0 && messages[i].lifetime > 0) {
                persistent_message_count++;
            }
        }

        // Verificar si se ha alcanzado el límite de 5 mensajes persistentes
        if (persistent_message_count >= 5) {
            send_response(request->client_pipe, "Error: Se ha alcanzado el límite de 5 mensajes persistentes en este tópico.");
            return;
        }
    }

    // Almacenar el mensaje
    if (message_count < MAX_MESSAGES) {
        // Guardar el mensaje en la estructura de mensajes
        strncpy(messages[message_count].topic, request->topic, sizeof(messages[message_count].topic) - 1);
        strncpy(messages[message_count].username, request->username, sizeof(messages[message_count].username) - 1);
        strncpy(messages[message_count].message, request->message, sizeof(messages[message_count].message) - 1);
        messages[message_count].lifetime = request->lifetime; // lifetime restante
        message_count++;

        // Marcar que el tópico ahora tiene mensajes activos
        topics[topic_index].has_active_messages = 1;
        // Enviar el mensaje a los suscriptores excepto al remitente
        char formatted_message[1028]; // espacio para el formato
        snprintf(formatted_message, sizeof(formatted_message), "%s %s %s",
         request->topic, request->username, request->message);

        // Enviar el mensaje a los suscriptores excepto al remitente
        for (int i = 0; i < topics[topic_index].subscriber_count; i++) {
            const char *subscriber_username = topics[topic_index].subscribers[i];
            if (strcmp(subscriber_username, request->username) != 0) { // evitar al remitente
                for (int j = 0; j < client_count; j++) {
                    if (strcmp(clients[j].username, subscriber_username) == 0) {
                        send_response(clients[j].client_pipe, formatted_message);
                    }
                }
            }
        }

        // Guardar el mensaje en el archivo si es persistente
        if (request->lifetime > 0) {
            const char* msg_file = getenv("MSG_FICH");
            if (msg_file) {
                FILE* file = fopen(msg_file, "a");
                if (file) {
                    fprintf(file, "%s %s %d %s\n",
                            request->topic, request->username, request->lifetime, request->message);
                    fclose(file);
                } else {
                    perror("Error al abrir el archivo de mensajes");
                }
            } else {
                perror("Variable de entorno MSG_FICH no configurada");
            }
        }

        // Imprimir el mensaje en la consola
        printf("Mensaje de %s enviado al tópico %s\n", request->username, request->topic);

        // Enviar una respuesta al cliente que envió el mensaje
        send_response(request->client_pipe, "Mensaje enviado con éxito.");
    } else {
        send_response(request->client_pipe, "Error: máximo de mensajes alcanzado.");
    }
}



// Función para cargar los mensajes cuyo lifetime sea mayor a 0 desde el archivo
int load_messages() {
    const char* msg_file = getenv("MSG_FICH"); // obtener el archivo desde la variable de entorno
    if (!msg_file) {
        perror("Variable de entorno MSG_FICH no configurada");
        return 0;
    }

    FILE* file = fopen(msg_file, "r"); // abrir el archivo para lectura
    if (!file) {
        perror("Error al abrir el archivo de mensajes para lectura");
        return 0;
    }

    int loaded_count = 0;
    while (fscanf(file, "%s %s %d %[^\n]", 
                   messages[loaded_count].topic, 
                   messages[loaded_count].username, 
                   &messages[loaded_count].lifetime, 
                   messages[loaded_count].message) == 4) {
        // Solo cargar los mensajes cuyo lifetime sea mayor a 0
        if (messages[loaded_count].lifetime > 0) {
            // Verificar si el tópico ya existe
            int topic_exists = 0;
            for (int i = 0; i < topic_count; i++) {
                if (strcmp(topics[i].name, messages[loaded_count].topic) == 0) {
                    topic_exists = 1;
                    topics[i].has_active_messages = 1; // marcamos que el tópico tiene mensajes activos
                    break;
                }
            }

            // Si el tópico no existe, agregarlo
            if (!topic_exists) {
                // Añadir un nuevo tópico al arreglo
                strcpy(topics[topic_count].name, messages[loaded_count].topic);
                topics[topic_count].has_active_messages = 1; // tópico con mensaje activo
                topic_count++;
            }

            loaded_count++; // incrementar el contador si el mensaje es válido
        }
    }

    fclose(file); // cerrar el archivo después de leer
    return loaded_count; // retornar el número de mensajes cargados
}


// Función que maneja la señal SIGUSR1 (eliminación de hilos)
void thread_signal_handler(int sig) {
    if (sig == SIGUSR1) {
        terminate_thread = 1;
    }
}

// Función para disminuir el lifetime de los mensajes cada segundo y almacenar solamente los mensajes persistentes en el archivo
void* manage_lifetime(void* arg) {
    struct sigaction sa;
    sa.sa_handler = thread_signal_handler;  // registrar el manejador de señales
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);  // asignar el manejador para SIGUSR1

    // Cargar los mensajes si se reinicia el manager y había alguno en el archivo
    message_count = load_messages(); // cargar mensajes desde el archivo

    while (!terminate_thread) {
        sleep(1);  // esperar 1 segundo para actualizar el archivo

        // Decrementar el lifetime de los mensajes
        for (int i = 0; i < message_count; i++) {
            if (messages[i].lifetime > 0) {
                messages[i].lifetime--;  // decrementar el lifetime
            }
        }

        // Eliminar mensajes con lifetime == 0
        int new_message_count = 0;
        for (int i = 0; i < message_count; i++) {
            if (messages[i].lifetime > 0) {
                messages[new_message_count] = messages[i];
                new_message_count++;
            }
        }
        message_count = new_message_count;  // actualizar el contador de mensajes

        // Comprobar si algún tópico tiene mensajes activos
        for (int i = 0; i < topic_count; i++) {
            int topic_has_active_messages = 0;
            for (int j = 0; j < message_count; j++) {
                if (strcmp(topics[i].name, messages[j].topic) == 0 && messages[j].lifetime > 0) {
                    topic_has_active_messages = 1;
                    break;
                }
            }
            topics[i].has_active_messages = topic_has_active_messages;
        }

        // Eliminar tópicos sin mensajes activos y sin suscriptores
        for (int i = 0; i < topic_count; i++) {
            if (!topics[i].has_active_messages && topics[i].subscriber_count == 0) {
                for (int j = i; j < topic_count - 1; j++) {
                    topics[j] = topics[j + 1];  // desplazar los tópicos
                }
                topic_count--;  // reducir el contador de tópicos
                i--;  // ajustar el índice
            }
        }

        // Reescribir el archivo solo con los mensajes con lifetime > 0
        const char* msg_file = getenv("MSG_FICH");
        if (!msg_file) {
            perror("Variable de entorno MSG_FICH no configurada");
            return NULL;
        }

        FILE* file = fopen(msg_file, "w");
        if (file) {
            for (int i = 0; i < message_count; i++) {
                if (messages[i].lifetime > 0) {
                    fprintf(file, "%s %s %d %s\n",
                            messages[i].topic,
                            messages[i].username,
                            messages[i].lifetime,
                            messages[i].message);
                }
            }
            fclose(file);
        } else {
            perror("Error al abrir el archivo de mensajes para reescritura");
        }
    }
    pthread_exit(NULL); // finaliza el hilo
}

// Función para eliminar un cliente de la sesión actual
void remove_client(const char *username) {
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].username, username) == 0) {
            // Enviar la señal SIGTERM al proceso del cliente para finalizar su proceso
            if (clients[i].pid > 0) {
                kill(clients[i].pid, SIGTERM);
                printf("Se envió SIGTERM a %s (PID: %d)\n", username, clients[i].pid);
            }
            // Desplazar elementos hacia atrás para eliminar al cliente
            for (int j = i; j < client_count - 1; j++) {
                clients[j] = clients[j + 1];
            }
            client_count--; // reducir el contador de clientes
            printf("Cliente '%s' ha sido eliminado de la lista de conectados.\n", username);
            char formatted_message[100];
            snprintf(formatted_message, sizeof(formatted_message), "El  cliente '%s' ha sido eliminado de la lista de conectados.\n", username);  
            // Notificar a los clientes conectados
            for (int i = 0; i < client_count; i++) {
                send_response(clients[i].client_pipe, formatted_message);
            }
            return;
        }
    }
    printf("Cliente '%s' no encontrado.\n", username);
}

// Función para mostrar los mensajes de un topico
void show_messages(const char *topic_name) {
    // Comprobar si el tópico existe
    if (!topic_exists(topic_name)) {
        printf("El tópico '%s' no existe.\n", topic_name);
        return;
    }

    int found_messages = 0; // contador para verificar si hay mensajes
    const char *msg_file = getenv("MSG_FICH");  // obtener el nombre del archivo desde la variable de entorno

    if (msg_file == NULL) {
        printf("La variable de entorno MSG_FICH no está configurada.\n");
        return;
    }

    FILE *file = fopen(msg_file, "r");  // abrir el archivo en modo lectura
    if (file == NULL) {
        perror("Error al abrir el archivo de mensajes");
        return;
    }

    char line[512];  // buffer para leer cada línea del archivo
    while (fgets(line, sizeof(line), file)) {
        StoredMessage msg;
        // Leer los datos de la línea
        int n = sscanf(line, "%255s %255s %d %300[^\n]", msg.topic, msg.username, &msg.lifetime, msg.message);
        if (n != 4) {
            continue;  // Si la línea no tiene el formato correcto, pasar a la siguiente
        }

        // Comprobar si el mensaje pertenece al tópico dado
        if (strcmp(msg.topic, topic_name) == 0) {
            found_messages = 1; // se encontraron mensajes
            printf("Usuario: %s, Mensaje: %s\n", msg.username, msg.message);  // imprimir información del mensaje
        }
    }

    fclose(file);  // Cerrar el archivo

    if (!found_messages) {
        printf("No hay mensajes en el tópico '%s'.\n", topic_name);
    }
}


// Función para manejar el CTRL+C del cliente
void handle_ctrlc(const char *username) {
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].username, username) == 0) {
            // Enviar la señal SIGTERM al proceso del cliente para finalizar su proceso
            if (clients[i].pid > 0) {
                kill(clients[i].pid, SIGINT);
                printf("Se envió SIGINT a %s (PID: %d)\n", username, clients[i].pid);
            }
            // Desplazar elementos hacia atrás para eliminar al cliente
            for (int j = i; j < client_count - 1; j++) {
                clients[j] = clients[j + 1];
            }
            client_count--; // reducir el contador de clientes
            printf("Cliente '%s' ha sido eliminado de la lista de conectados.\n", username);
            return;
        }
    }
    printf("Cliente '%s' no encontrado.\n", username);
}

// Función para bloquear el envío de mensajes en un topico
void lock_topic(const char *topic_name) {
    for (int i = 0; i < topic_count; i++) {
        if (strcmp(topics[i].name, topic_name) == 0) {
            if (!topics[i].is_locked) {
                topics[i].is_locked = 1; // bloquear el tópico
                printf("Tópico '%s' bloqueado.\n", topic_name);

                // Notificar a los suscriptores del bloqueo
                char notification[256];
                snprintf(notification, sizeof(notification), "El tópico '%s' ha sido bloqueado. No se pueden enviar mensajes temporalmente.", topic_name);
                for (int j = 0; j < topics[i].subscriber_count; j++) {
                    for (int k = 0; k < client_count; k++) {
                        if (strcmp(clients[k].username, topics[i].subscribers[j]) == 0) {
                            send_response(clients[k].client_pipe, notification);
                            break;
                        }
                    }
                }
            } else {
                printf("El tópico '%s' ya está bloqueado.\n", topic_name);
            }
            return;
        }
    }
    printf("No se encontró el tópico '%s' para bloquear.\n", topic_name);
}

// Función para bloquear el envío de mensajes en un topico
void unlock_topic(const char* topic_name) {
    for (int i = 0; i < topic_count; i++) {
        if (strcmp(topics[i].name, topic_name) == 0) {
            if (topics[i].is_locked) {
                topics[i].is_locked = 0;  // desbloquear el tópico
                printf("El tópico '%s' ha sido desbloqueado para el envío de mensajes.\n", topic_name);

                // Notificar a los suscriptores del desbloqueo
                char notification[256];
                snprintf(notification, sizeof(notification), "El tópico '%s' ha sido desbloqueado. Ya puedes enviar mensajes.", topic_name);
                for (int j = 0; j < topics[i].subscriber_count; j++) {
                    for (int k = 0; k < client_count; k++) {
                        if (strcmp(clients[k].username, topics[i].subscribers[j]) == 0) {
                            send_response(clients[k].client_pipe, notification);
                            break;
                        }
                    }
                }
            } else {
                printf("El tópico '%s' ya está desbloqueado.\n", topic_name);
            }
            return;
        }
    }
    printf("Error: Tópico '%s' no encontrado.\n", topic_name);
}


// Función para manejar el envío de comandos del manager
void* command_sender(void* arg) {
    struct sigaction sa;
    sa.sa_handler = thread_signal_handler; // registrar el manejador de señales
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);  // asignar el manejador para SIGUSR1

    char input[256];
    while (!terminate_thread) {     
        if (fgets(input, sizeof(input), stdin) == NULL) {
            if (terminate_thread){
                printf("Recibida señal de terminación\n");
                break; 
            }  
            continue;
        }
        input[strcspn(input, "\n")] = 0; // eliminar salto de línea para que se pueda procesar bien el comando

        // Comando remove <user>
        if (strncmp(input, "remove ", 7) == 0) {
            char username[USERNAME_LEN];
            sscanf(input + 7, "%s", username);
            pthread_mutex_lock(&mutex);
            remove_client(username); // Eliminar cliente
            pthread_mutex_unlock(&mutex);
        }
        // Comando close
        else if (strcmp(input, "close") == 0) {
            close_all_connections(); 
            unlink(SERVER_PIPE); 
            exit(0); 
        }
        // Comando users
        else if (strcmp(input, "users") == 0) {
            pthread_mutex_lock(&mutex);
            printf("Lista de usuarios conectados:\n");
            list_connected_users();
            pthread_mutex_unlock(&mutex);
        }
        // Comando topics
        else if (strcmp(input, "topics") == 0) {
            pthread_mutex_lock(&mutex);
            printf("Tópicos:\n");
            if (topic_count == 0) {
                printf("No se encontraron tópicos para listar.\n");
            }
            else{
                for (int i = 0; i < topic_count; i++) {
                printf(" - %s (Suscriptores: %d)\n", topics[i].name, topics[i].subscriber_count);
                }
            }
            pthread_mutex_unlock(&mutex);
        }
        // Comando lock <topic>
        else if (strncmp(input, "lock ", 5) == 0){
            char topic[TOPIC_NAME_LEN];
            sscanf(input + 5, "%s", topic);
            pthread_mutex_lock(&mutex);
            lock_topic(topic);
            pthread_mutex_unlock(&mutex);
        }
        // Comando unlock <topic>
        else if (strncmp(input, "unlock ", 7) == 0){
            char topic[TOPIC_NAME_LEN];
            sscanf(input + 7, "%s", topic);
            pthread_mutex_lock(&mutex);
            unlock_topic(topic);
            pthread_mutex_unlock(&mutex);
        }
        else {
            printf("Comando desconocido: %s\n", input);
        }
    }
    pthread_exit(NULL); // terminar el hilo
}


int main() {
    Response msg;
    
    // Definir el nombre de la variable de ambiente y el fichero donde se guardarán los mensajes
    const char *MSG_FICH = "MSG_FICH";
    const char *file_name = "mensajes.txt";
    
    // Usar setenv para establecer la variable de entorno
    if (setenv(MSG_FICH, file_name, 1) != 0) {
        perror("Error al establecer la variable de entorno");
        return 1;
    }
    // Cargar los mensajes del fichero del manager anterior
    load_messages();

    // Configurar el manejador de señal para SIGINT
    signal(SIGINT, handle_sigint);
    // Configurar el manejador de señal para SIGUSR1
    signal(SIGUSR1, thread_signal_handler);

    // Comprobar que solo hay un manager en ejecución
    if (access(SERVER_PIPE, F_OK) == 0){
        printf("YA HAY UN SERVIDOR EN EJECUCIÓN\n");
        exit(1);
    }

    // Crear la pipe del servidor
    mkfifo(SERVER_PIPE, 0600);

    // Inicializar el mutex
    pthread_mutex_init(&mutex, NULL); 

    // Iniciar el hilo para gestionar el lifetime de los mensajes
    if (pthread_create(&lifetime_thread, NULL, manage_lifetime, NULL) != 0) {
        perror("Error al crear el hilo de gestión de lifetime");
        return 1;
    }

    // Crea un hilo para ejecutar los comandos ya que el hilo principal escucha los comandos del cliente
    if (pthread_create(&command_thread, NULL, command_sender, NULL) != 0) {
        perror("Error al crear el hilo de envío de comandos");
        return 1;
    }

    // Texto inicial
    printf("Esperando conexiones...\n");

    while (!terminate_thread) {
        // Esperar por un mensaje del cliente
        int fd = open(SERVER_PIPE, O_RDONLY);
        if (fd == -1) {
            perror("Error al abrir la pipe del servidor");
            continue; // Volver a intentar en el siguiente ciclo
        }
        // Leer la solicitud del cliente
        ssize_t bytesRead = read(fd, &msg, sizeof(Response));
        if (bytesRead < 0) {
            perror("Error al leer el mensaje del cliente");
            close(fd);
            continue; // Volver a intentar en el siguiente ciclo
        } else if (bytesRead == 0) {
            close(fd); // Uso para evitar duplicar el mensaje de bienvenida
            continue; // Volver a intentar en el siguiente ciclo
        }

        // Se bloquea el mutex
        pthread_mutex_lock(&mutex);

        switch (msg.command_type) {
            // Mensaje de conexión
            case 0: 
                char res[512];
                if (client_count < MAX_USERS) {
                    int duplicate_found = 0; 
                    // Verificar si el nombre de usuario ya está en uso
                    for (int i = 0; i < client_count; i++) {
                        if (strcmp(clients[i].username, msg.username) == 0) {
                            printf("ERR: Username '%s' is already in use.\n", msg.username);
                            duplicate_found = 1;
                            sprintf(res, "ERR: Username '%s' is already in use.\n", msg.username);
                            send_response(msg.client_pipe, res);
                            sleep(1);
                            kill(msg.pid, SIGTERM); // cierra el nuevo cliente
                        }
                    }

                    // Si no se encuentra un duplicado, agregar al nuevo cliente
                    if (duplicate_found == 0) {
                        if (msg.username[0] != '\0') { // verificar que el nombre no esté vacío
                            sprintf(res, "Bienvenido, %s", msg.username);
                            send_response(msg.client_pipe, res);
                            add_client(msg.client_pipe, msg.username, msg.pid);
                        } else {
                            printf("ERR: Invalid username.\n");
                            send_response(msg.client_pipe, "ERR: Invalid username.\n");
                            sleep(1);
                            kill(msg.pid, SIGTERM); 
                        }
                    }
                } else {            
                    printf("ERR: Max number of users reached (%d).\n", MAX_USERS);
                    sprintf(res, "ERR: Max number of users reached (%d).\n", MAX_USERS);
                    send_response(msg.client_pipe, res);
                    sleep(1);
                    kill(msg.pid, SIGTERM);
                }
            break;

            // Manejo de la creación de un tópico
            case 1: 
                subscribe_topic(msg.topic, msg.client_pipe, msg.username);
                break;

            // Manejo de listar los topicos
            case 2:
                printf("Listar tópicos para el usuario '%s'.\n", msg.username);
                list_topics(msg.client_pipe);
                break;

            // Manejo del comando exit del cliente
            case 3:
                printf("Cliente '%s' ha salido.\n", msg.username);
                remove_client(msg.username);
                break;
                
            // Manejo de la desuscripcion de un cliente en un topico
            case 4:
                printf("El usuario '%s'se ha desuscrito del tópico '%s'\n", msg.username, msg.topic);
                unsubscribe_topic(msg.topic, msg.client_pipe, msg.username);
                break;

            // Manejo del envío de un mensaje y almacenamiento en un archivo si es persistente
            case 5:
                send_message(&msg);
                break;

            // Manejo del CTRL+C del cliente
            case 6:
                handle_ctrlc(msg.username);
                break;
                
            default:
                // Enviar respuesta de comando no reconocido
                int client_fd = open(msg.client_pipe, O_WRONLY);
                write(client_fd, "Comando no reconocido.", 22);
                close(client_fd);
                printf("Comando no reconocido: tipo %d\n", msg.command_type);
                break;
        }

        pthread_mutex_unlock(&mutex); // Desbloquear el mutex después de acceder a la sección crítica
    }
    return 0;
}