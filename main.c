#include <stdio.h>



#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <wait.h>
#include <fcntl.h>
#include <dirent.h>

#define READ_STRING(descriptor, string)  \
{ \
    int length = 0;                        \
    read ( descriptor, & length, sizeof ( int ) ); \
    read ( descriptor, string, length );   \
}

#define WRITE_STRING(descriptor, string) \
{ \
    int length = strlen ( string ) + 1;     \
    write ( descriptor, & length, sizeof ( int ) ); \
    write ( descriptor, string, length );   \
}

#define STR_EQ(str1, str2) ( strcmp ( str1, str2 ) == 0)

#define PARENT(pid) ( pid > 0 )
#define CHILD(pid) ( pid == 0 )

int fifo;
int pipeCtoS[2];
int sockets[2];

pid_t pid;

void find ( char * currentPath, char * fileName, char * finalFilePath )
{
    if ( strlen ( finalFilePath ) > 0 )
        return;

    DIR * directory = opendir ( currentPath );

    if ( directory != NULL )
    {

        struct dirent * entry = readdir ( directory );
        while ( entry != NULL && ( STR_EQ ( entry->d_name, "." ) || STR_EQ( entry->d_name, ".." ) ) )
        {
            entry = readdir ( directory );
        }

        while ( entry != NULL )
        {
            if ( entry->d_type & DT_DIR )
            {
                char nextPath [ 1024 ];
                strcpy ( nextPath, currentPath );
                strcat ( nextPath, "/" );
                strcat ( nextPath, entry->d_name );

                find ( nextPath, fileName, finalFilePath );
            }
            if ( STR_EQ ( fileName, entry->d_name ) )
            {
                strcpy ( finalFilePath, currentPath );
                strcat ( finalFilePath, "/" );
                strcat ( finalFilePath, entry->d_name );
                return;
            }


            entry = readdir( directory );
            while ( entry != NULL && ( STR_EQ ( entry->d_name, "." ) || STR_EQ( entry->d_name, ".." ) ) )
            {
                entry = readdir(directory);
            }
        }

        closedir( directory );
    }
}

void replyToFind ( char file [] )
{
    char finalPath[1024];
    strcpy( finalPath, "" );

    find ("/home", file, finalPath);

    if ( strlen(finalPath) == 0 )
    {
        WRITE_STRING(sockets[0], "not-found")
    }
    else
    {
        WRITE_STRING(sockets[0], finalPath)
    }
}

void replyToStat ( char filePath [] )
{
    if ( STR_EQ ( filePath, "abort-stat") )
    {
        return;
    }

    struct stat st;
    if ( -1 == stat ( filePath, & st ) )
    {
        WRITE_STRING(sockets[0], "stat-error")
    }
    else
    {
        WRITE_STRING(sockets[0], strrchr ( filePath, '/' ) + 1)

        int size = st.st_size;
        int uid = st.st_uid;
        int gid = st.st_gid;
        int blocks = st.st_blocks;
        int permissions = 000;

        char fileType [ 256 ];
        strcpy ( fileType, "unknown" );

        if ( S_ISDIR( st.st_mode ) ) strcpy ( fileType, "directory" );
        if ( S_ISBLK( st.st_mode ) ) strcpy ( fileType, "block device" );
        if ( S_ISCHR( st.st_mode ) ) strcpy ( fileType, "character device" );
        if ( S_ISFIFO( st.st_mode ) ) strcpy ( fileType, "fifo/pipe" );
        if ( S_ISLNK( st.st_mode ) ) strcpy ( fileType, "symbolic link" );
        if ( S_ISREG( st.st_mode ) ) strcpy ( fileType, "regular file" );
        if ( S_ISSOCK( st.st_mode ) ) strcpy ( fileType, "socket" );

        if ( st.st_mode & S_IRUSR ) permissions = permissions + 100 * 4;
        if ( st.st_mode & S_IWUSR ) permissions = permissions + 100 * 2;
        if ( st.st_mode & S_IXUSR ) permissions = permissions + 100 * 1;
        if ( st.st_mode & S_IRGRP ) permissions = permissions + 10 * 4;
        if ( st.st_mode & S_IWGRP ) permissions = permissions + 10 * 2;
        if ( st.st_mode & S_IXGRP ) permissions = permissions + 10 * 1;
        if ( st.st_mode & S_IROTH ) permissions = permissions + 1 * 4;
        if ( st.st_mode & S_IWOTH ) permissions = permissions + 1 * 2;
        if ( st.st_mode & S_IXOTH ) permissions = permissions + 1 * 1;

        write ( sockets[0], & size, sizeof ( int ) );
        write ( sockets[0], & uid, sizeof ( int ) );
        write ( sockets[0], & gid, sizeof ( int ) );
        write ( sockets[0], & blocks, sizeof ( int ) );

        WRITE_STRING( sockets[0], fileType )

        write ( sockets[0], & permissions, sizeof (int) );

    }
}

void waitForFindResponse ()
{
    char reply[1024];
    READ_STRING(sockets[1], reply)
    printf("Found file at : %s\n", reply);
}

void waitForStatResponse ()
{
    char reply [256];

    READ_STRING( sockets[1], reply )

    if ( STR_EQ ( reply, "stat-error" ) )
    {
        printf("Invalid path given / no permissions\n");
    }
    else
    {
        printf("Stats for file '%s' :\n", reply);

        int size;
        int uid;
        int gid;
        int blocks;
        int permissions;
        char fileType [ 256 ];

        read ( sockets[1], & size, sizeof (int) );
        read ( sockets[1], & uid, sizeof (int) );
        read ( sockets[1], & gid, sizeof (int) );
        read ( sockets[1], & blocks, sizeof (int) );

        READ_STRING( sockets[1], fileType );

        read ( sockets[1], & permissions, sizeof (int) );

        printf("\tSize : %d\tBlocks : %d\tFile Type : %s\n", size, blocks, fileType);
        printf("\tAccess : %d\tUser id : %d\tGroup id : %d\n", permissions, uid, gid);
    }
}

void loopClient()
{
    char command[256];
    char file[256];

    while (1)
    {
        printf("Type in your command below\n");

        scanf ( "%s", command );

        WRITE_STRING( sockets[1], command )

        if ( STR_EQ ( command, "quit" ) )
        {
            return;
        }

        if ( STR_EQ ( command, "myfind" ) )
        {
            scanf ( "%s", file );
            WRITE_STRING(sockets[1], file)

            waitForFindResponse ();
        }
        else if ( STR_EQ ( command, "mystat" ) )
        {
            scanf ( "%s", file );

            if ( strchr ( file, '/' ) == NULL )
            {
                WRITE_STRING(sockets[1], "abort-stat")
                printf("Invalid file path format\n");
            }
            else
            {
                WRITE_STRING(sockets[1], file)

                waitForStatResponse ();
            }
        }
    }
}

void loopServer()
{
    char clientCommandMessage [ 256 ];
    char clientFileMessage [ 256 ];

    while (1)
    {
        READ_STRING( sockets[0], clientCommandMessage )

        if ( STR_EQ ( clientCommandMessage, "quit" ) )
        {
            return;
        }

        if ( STR_EQ ( clientCommandMessage, "myfind" ) )
        {
            READ_STRING( sockets[0], clientFileMessage )
            replyToFind ( clientFileMessage );
        }

        if ( STR_EQ ( clientCommandMessage, "mystat" ) )
        {
            READ_STRING( sockets[0], clientFileMessage )
            replyToStat ( clientFileMessage );
        }
    }
}

int usernameExists ( char username [] )
{
    FILE * users = fopen ( "users.txt", "r" );

    char usernameInFile [ 256 ];

    while ( fscanf ( users, "%s", usernameInFile ) != -1 )
    {
        if ( STR_EQ ( usernameInFile, username ) )
        {
            return 1;
        }
    }

    fclose ( users );
    return 0;
}

void loginClient ()
{
    char consoleCommand [ 256 ];
    char serverResponse [ 256 ];

    while ( 1 )
    {
        printf( "login : " );
        fflush( stdout );

        scanf ( "%s", consoleCommand );

        WRITE_STRING(pipeCtoS[1], consoleCommand)
        if ( STR_EQ ( consoleCommand, "quit" ) )
        {
            return;
        }

        READ_STRING(fifo, serverResponse)
        if ( STR_EQ ( serverResponse, "login-good" ) )
        {
            return;
        }
    }
}

void client()
{
    close ( pipeCtoS [0] );
    close ( sockets[0] );

    fifo = open ( "fifo", O_RDONLY );

    loginClient();
    printf("Logged in\n");
    loopClient();

    close ( fifo );

    close ( pipeCtoS[1] );
    close ( sockets[1] );
}

void loginServer ()
{
    char clientMessage [ 256 ];

    while ( 1 )
    {
        READ_STRING( pipeCtoS[0], clientMessage )

        if ( STR_EQ(clientMessage, "quit" ) )
        {
            return;
        }

        if ( usernameExists ( clientMessage ) )
        {
            WRITE_STRING(fifo, "login-good")
            return;
        }

        WRITE_STRING(fifo, "login-bad")
    }
}

void server()
{
    close ( pipeCtoS[1] );
    close ( sockets[1] );

    fifo = open ( "fifo", O_WRONLY );

    loginServer();
    loopServer();

    close ( fifo );

    close ( pipeCtoS[0] );
    close ( sockets[0] );
}

int main()
{
    mkfifo( "fifo", 0666 );
    pipe(pipeCtoS);
    socketpair( AF_UNIX, SOCK_STREAM, PF_UNSPEC, sockets );

    pid = fork();

    if ( CHILD(pid) )
    {
        server();
    }
    else if ( PARENT(pid) )
    {
        client();
        wait ( NULL );
    }

    return 0;
}
