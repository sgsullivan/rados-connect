#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "rados/librados.h"
#include "rbd/librbd.h"


int _radosAuthPermChecks( char* pool, char* user, char* key, char* monips );
void _usage();

void main (int argc, char** argv) {

    int m, n, l, x, ch;
    char mons[256];
    char pool[64];
    char cephx_user[64];
    char cephx_key[256];
    char libvir_uuid[256];
    int mons_given = 0;
    int pool_given = 0;
    int cephx_user_given = 0;
    int cephx_key_given = 0;
    int libvir_uuid_given = 0;

    // Parse and populate cmdline args.
    for ( n = 1; n < argc; n++ ) {
        switch ( (int)argv[n][0] ) {
            case '-': x = 0;
                l = strlen( argv[n] );
                for ( m = 1; m < l; ++m ) {
                    ch = (int)argv[n][m];
                    switch( ch ) {
                        case 'm':
                            if ( m + 1 >= l ) {
                                fprintf(stderr, "Illegal syntax -- no MONS given!\n" );
                                exit(EXIT_FAILURE);
                            }
                            else {
                                strcpy( mons, &argv[n][m+1] );
                                mons_given = 1;
                            }
                            x = 1;
                            break;
                        case 'p':
                            if ( m + 1 >= l ) {
                                fprintf(stderr, "Illegal syntax -- no pool given!\n" );
                                exit(EXIT_FAILURE);
                            }
                            else {
                                strcpy( pool, &argv[n][m+1] );
                                pool_given = 1;
                            }
                            x = 1;
                            break;
                        case 'u':
                            if ( m + 1 >= l ) {
                                fprintf(stderr, "Illegal syntax -- no cephx user given!\n" );
                                exit(EXIT_FAILURE);
                            }
                            else {
                                strcpy( cephx_user, &argv[n][m+1] );
                                cephx_user_given = 1;
                            }
                            x = 1;
                            break;
                        case 'k':
                            if ( m + 1 >= l ) {
                                fprintf(stderr, "Illegal syntax -- no cephx key given!\n" );
                                exit(EXIT_FAILURE);
                            }
                            else {
                                strcpy( cephx_key, &argv[n][m+1] );
                                cephx_key_given = 1;
                            }
                            x = 1;
                            break;
                        case 'l':
                            if ( m + 1 >= l ) {
                                fprintf(stderr, "Illegal syntax -- no libvirt UUID given!\n" );
                                exit(EXIT_FAILURE);
                            }
                            else {
                                strcpy( libvir_uuid, &argv[n][m+1] );
                                libvir_uuid_given = 1;
                            }
                            x = 1;
                            break;
                        case 'h':
                            _usage();
                            exit(EXIT_SUCCESS);
                        default:
                            fprintf(stderr, "Illegal option code = %c\n", ch );
                            exit(EXIT_FAILURE);
                    }
                    if ( x == 1 )
                        break;
                }
                break;
            default:
                fprintf(stderr, "Illegal syntax\n" );
                exit(EXIT_FAILURE);
            }
    }            


    if ( cephx_key_given && libvir_uuid_given ) {
        fprintf(stderr, "Conflicting options: -k & -l\n");
        exit(EXIT_FAILURE);
    }

    if ( ! pool_given || ! cephx_user_given || ! mons_given || ! cephx_key_given && ! libvir_uuid_given ) {
        fprintf(stderr, "\nERROR: Missing required arguments!\n");
        _usage();
        exit(EXIT_FAILURE);
    }

    if ( cephx_key_given ) {
        printf("\nVerifying cluster connectivity and permissions via given cephx key...\n");
        goto radosauthpermcks;
    }
    else if ( libvir_uuid_given ) {
        printf("\nFinding cephx key for libvirt UUID [%s]\n", libvir_uuid);

        char libvir_uuid_file_path_buf[256] = "/etc/libvirt/secrets/";
        strcat(libvir_uuid_file_path_buf, libvir_uuid);
        strcat(libvir_uuid_file_path_buf, ".base64");

        FILE *fp;
        fp = fopen(libvir_uuid_file_path_buf , "r");
        if (fp == NULL) {
            fprintf(stderr, "Unable to open [%s]!\n", libvir_uuid_file_path_buf);
            exit(EXIT_FAILURE);
        }
        if ( fgets (cephx_key, 256, fp)==NULL ) {
            fprintf(stderr, "Unable to read [%s]!\n", libvir_uuid_file_path_buf);
            exit(EXIT_FAILURE);
        }
        fclose(fp);
        strtok(cephx_key, "\n");

        printf("Discovered cephx key from libvirt UUID\n");
        printf("Verifying cluster connectivity and permissions via given libvirt UUID...\n");
        goto radosauthpermcks;
    }

radosauthpermcks:
    if ( _radosAuthPermChecks(pool, cephx_user, cephx_key, mons) < 0 ) {
        fprintf(stderr, "FATAL: Authentication error or insufficient permissions.\n\n");
        exit(EXIT_FAILURE);
    }
    else {
        printf("No errors were encountered!\n\n");
        exit(EXIT_SUCCESS);
    }
}


int _radosAuthPermChecks( char* pool, char* user, char* key, char* monips ) {

    // Default, return good.
    int ret = 0;

    // instantiate cluster handle
    int err;
    rados_t cluster;
    
    // Create cluster handle, with user passed.
    err = rados_create(&cluster, user);
    if (err < 0) {
        fprintf(stderr, "\tBAD: Cannot create a cluster handle: %s\n", strerror(-err));
        ret = -1;
        goto cleanup;
    }

    // Set given key
    if (rados_conf_set(cluster, "key", key) < 0) {
        fprintf(stderr, "\tBAD: Failed to set RADOS option [key]\n");
        ret = -1;
        goto cleanup;
    }
    
    // configure rados_t to connect to cluster, based on args given.
    if (rados_conf_set(cluster, "auth_supported", "cephx") < 0) {
        fprintf(stderr, "\tBAD: Failed to set RADOS option auth_supported [cephx]\n");
        ret = -1;
        goto cleanup;
    }
    
    // Set given MONS
    if (rados_conf_set(cluster, "mon_host", monips) < 0) {
        fprintf(stderr, "\tBAD: Failed to set RADOS option [mons]\n");
        ret = -1;
        goto cleanup;
    }

    /* 
      * Set our connection timeouts. Default is 5 minutes, change this to 30 seconds. 
      * 5 minutes seems like to long to wait to staul out an attachment workflow 
      * before giving up.
      */
    const char *client_mount_timeout = "30";
    const char *mon_op_timeout = "30";
    const char *osd_op_timeout = "30";    
    rados_conf_set(cluster, "client_mount_timeout", client_mount_timeout);
    rados_conf_set(cluster, "rados_mon_op_timeout", mon_op_timeout);
    rados_conf_set(cluster, "rados_osd_op_timeout", osd_op_timeout);    

    // connect to cluster
    err = rados_connect(cluster);
    if (err < 0) {
        fprintf(stderr, "\tBAD: Cannot connect to cluster: [%s]\n", strerror(-err));
        ret = -1;
        goto cleanup;
    }
    
    // open an I/O context on given pool
    rados_ioctx_t io;
    err = rados_ioctx_create(cluster, pool, &io);
    if (err < 0) {
        fprintf(stderr, "\tBAD: Cannot open RADOS pool [%s]: [%s]\n", pool, strerror(-err));
        ret = -1;
        goto cleanup;
    }
    
    // Verify given pool exists
    int id = rados_pool_lookup(cluster, pool);
    if (id < 0) {
        fprintf(stderr, "BAD: Unable to get ID of pool [%s], does it exist? [%s]\n", pool, strerror(-id));
        rados_ioctx_destroy(io);
        ret = -1;
        goto cleanup;
    }
    else {
        printf("\tGOOD: Able to get pool ID. Pool [%s] ID is [%i]\n", pool, id);
    }

    // Get random UUID, for later use as object_name
    FILE *fp;
    char uuid[60];
    fp = fopen("/proc/sys/kernel/random/uuid" , "r");
    if (fp == NULL) {
        fprintf(stderr, "Unable to open [/proc/sys/kernel/random/uuid]!\n");
        ret = -1;
        goto cleanup;
    }
    if( fgets (uuid, 60, fp)==NULL ) {
        fprintf(stderr, "Unable to read [/proc/sys/kernel/random/uuid]!\n");
        ret = -1;
        goto cleanup;
    }
    fclose(fp);
    // Note: strtok not thread safe; shouldn't matter for this use case.
    strtok(uuid, "\n");
    
    // Write test object synchronously
    const char *object_name = uuid;
    const char *object_desc = "rados_connect test object";
    err = rados_write(io, object_name, object_desc, 25, 0);
    if (err < 0) {
        fprintf(stderr, "\tBAD: Cannot write object [%s] to pool [%s]: [%s]\n", object_name, pool, strerror(-err));
        rados_ioctx_destroy(io);
        ret = -1;
        goto cleanup;
    }
    else {
        printf("\tGOOD: Able to write [%s] to object [%s] on pool [%s]\n", object_desc, object_name, pool);
    }
    
    // Read test object
    char read_object_buf[150];
    err = rados_read(io, object_name, read_object_buf, 25, 0);
    if (err < 0) {
        fprintf(stderr, "BAD: Cannot read object [%s] [%s]\n", object_name, strerror(-err));
        rados_ioctx_destroy(io);
        ret = -1;
        goto cleanup;
    }
    else {
        printf("\tGOOD: Able to read object [%s]. Contents: [%s]\n", object_name, read_object_buf);
    }
    
    // Remove test object
    err = rados_remove(io, object_name);
    if (err < 0) {
        fprintf(stderr, "BAD: Cannot remove object. [%s] [%s]\n", pool, strerror(-err));
        rados_ioctx_destroy(io);
        ret = -1;
        goto cleanup;
    }
    else {
        printf("\tGOOD: Able to remove object [%s].\n", object_name);
    }

    // If we made it this far, success! Go to cleanup.
    rados_ioctx_destroy(io);
    goto cleanup;

cleanup:
    rados_shutdown(cluster);
    return ret;

}

void _usage() {
    printf("\nUSAGE: ./rados_connect [OPTIONS]\n \
\nNOTE: All switches below are _required_ unless stated optional.\n \
\t-m -- MONS    (ex: -m10.30.177.4:6789,10.30.177.5:6789,10.30.177.6:6789)\n \
\t-u -- CEPHX_USER    (ex: -umycephxuser)\n \
\t-p -- CEPH_POOL    (ex: -ppool0)\n \
\t-k -- CEPHX_KEY [optional]    (ex: -kAQDJ+gtScGVSBRAA0QenPqsxWaml1jr9C1647w==)\n \
\t-l -- LIBVIR_UUID [optional]    (ex: -l04a8f230-1bd0-4536-a101-3bbd6253ce5c)\n \
\n*** Options -l and -k are optional, however at least one _must_ be defined.\n \
\n");
}
