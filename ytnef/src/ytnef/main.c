#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ytnef.h>
#include "config.h"

#define PRODID "PRODID:-//The Gauntlet//" PACKAGE_STRING "//EN\n"

TNEFStruct TNEF;
int verbose = 0;
int savefiles = 0;
int saveRTF = 0;
char *filepath = NULL;
void ProcessTNEF(TNEFStruct TNEF);
void SaveVCalendar(TNEFStruct TNEF);
void SaveVCard(TNEFStruct TNEF);
void SaveVTask(TNEFStruct TNEF);


void PrintHelp(void) {
    printf("Yerase TNEF Exporter v");
            printf(VERSION);
            printf("\n");
    printf("\n");
    printf("  usage: ytnef [-+vhf] <filenames>\n");
    printf("\n");
    printf("   -/+v - Enables/Disables verbose output\n");
    printf("          Multiple -v's increase the level of output\n");
    printf("   -/+f - Enables/Disables saving of attachments\n");
    printf("   -/+F - Enables/Disables saving of the message body as RTF\n");
    printf("   -h   - Displays this help message\n");
    printf("\n");
    printf("Example:\n");
    printf("  ytnef -v winmail.dat\n");
    printf("     Parse with verbose output, don't save\n");
    printf("  ytnef -f . winmail.dat\n");
    printf("     Parse and save all attachments to local directory (.)\n");
    printf("  ytnef -F -f . winmail.dat\n");
    printf("     Parse and save all attachments to local directory (.)\n");
    printf("     Including saving the message text to a RTF file.\n\n");
    printf("Send bug reports to ");
        printf(PACKAGE_BUGREPORT);
        printf("\n");

}


int main(int argc, char ** argv) {
    int index,i;

//    printf("Size of WORD is %i\n", sizeof(WORD));
//    printf("Size of DWORD is %i\n", sizeof(DWORD));
//    printf("Size of DDWORD is %i\n", sizeof(DDWORD));

    if (argc == 1) {
        printf("You must specify files to parse\n");
        PrintHelp();
        return -1;
    }
    
    for(i=1; i<argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 'v': verbose++;
                          break;
                case 'h': PrintHelp();
                          return;
                case 'f': savefiles = 1;
                          filepath = argv[i+1];
                          i++;
                          break;
                case 'F': saveRTF = 1;
                          break;
                default: 
                          printf("Unknown option '%s'\n", argv[i]);
            }
            continue;

        }
        if (argv[i][0] == '+') {
            switch (argv[i][1]) {
                case 'v': verbose--;
                          break;
                case 'f': savefiles = 0;
                          filepath = NULL;
                          break;
                case 'F': saveRTF = 0;
                          break;
                default: 
                          printf("Unknown option '%s'\n", argv[i]);
            }
            continue;

        }

        TNEFInitialize(&TNEF);
        TNEF.Debug = verbose;
        if (TNEFParseFile(argv[i], &TNEF) == -1) {
            printf("ERROR processing file\n");
            continue;
        }
        ProcessTNEF(TNEF);
        TNEFFree(&TNEF);
    }
}

void ProcessTNEF(TNEFStruct TNEF) {
    char *astring;
    variableLength *filename;
    variableLength *filedata;
    Attachment *p;
    TNEFStruct emb_tnef;
    int RealAttachment;
    int object;
    char ifilename[256];
    int i;
    FILE *fptr;

// First see if this requires special processing.
// ie: it's a Contact Card, Task, or Meeting request (vCal/vCard)
    if (TNEF.messageClass[0] != 0)  {
        if (strcmp(TNEF.messageClass, "IPM.Contact") == 0) {
            SaveVCard(TNEF );
            return;
        }
        if (strcmp(TNEF.messageClass, "IPM.Task") == 0) {
            SaveVTask(TNEF);
            return;
        }
    }
    if ((filename = MAPIFindUserProp(&(TNEF.MapiProperties), 
                        PROP_TAG(PT_STRING8,0x24))) != MAPI_UNDEFINED) {
        if (strcmp(filename->data, "IPM.Appointment") == 0) {
            SaveVCalendar(TNEF);
        }
    }

// Now process each attachment
    p = TNEF.starting_attach.next;
    while (p != NULL) {
        // Make sure it has a size.
        if (p->FileData.size > 0) {
            object = 1;           
            // See if the contents are stored as "attached data"
            //  Inside the MAPI blocks.
            if((filedata = MAPIFindProperty(&(p->MAPI), 
                                    PROP_TAG(PT_OBJECT, PR_ATTACH_DATA_OBJ))) 
                    == MAPI_UNDEFINED) {
                if((filedata = MAPIFindProperty(&(p->MAPI), 
                                    PROP_TAG(PT_BINARY, PR_ATTACH_DATA_OBJ))) 
                        == MAPI_UNDEFINED) {
                    // Nope, standard TNEF stuff.
                    filedata = &(p->FileData);
                    object = 0;
                }
            }
            // See if this is an embedded TNEF stream.
            RealAttachment = 1;
            if (object == 1) {
                // This is an "embedded object", so skip the
                // 16-byte identifier first.
                DWORD signature;
                memcpy(&signature, filedata->data+16, sizeof(DWORD));
                if (TNEFCheckForSignature(signature) == 0) {
                    // Has a TNEF signature, so process it.
                    TNEFInitialize(&emb_tnef);
                    emb_tnef.Debug = TNEF.Debug;
                    if (TNEFParseMemory(filedata->data+16, 
                            filedata->size-16, &emb_tnef) != -1) {
                        ProcessTNEF(emb_tnef);
                        RealAttachment = 0;
                    }
                    TNEFFree(&emb_tnef);
                }
            } else {
                DWORD signature;
                memcpy(&signature, filedata->data, sizeof(DWORD));
                if (TNEFCheckForSignature(signature) == 0) {
                    // Has a TNEF signature, so process it.
                    TNEFInitialize(&emb_tnef);
                    emb_tnef.Debug = TNEF.Debug;
                    if (TNEFParseMemory(filedata->data, 
                            filedata->size, &emb_tnef) != -1) {
                        ProcessTNEF(emb_tnef);
                        RealAttachment = 0;
                    }
                    TNEFFree(&emb_tnef);
                }
            }
            if (RealAttachment == 1) {
                // Ok, it's not an embedded stream, so now we
                // process it.
                if ((filename = MAPIFindProperty(&(p->MAPI), 
                                        PROP_TAG(30,0x3707))) 
                        == MAPI_UNDEFINED) {
                    if ((filename = MAPIFindProperty(&(p->MAPI), 
                                        PROP_TAG(30,0x3001))) 
                            == MAPI_UNDEFINED) {
                        filename = &(p->Title);
                    }
                }
                if (filepath == NULL) {
                    sprintf(ifilename, "%s", filename->data);
                } else {
                    sprintf(ifilename, "%s/%s", filepath, filename->data);
                }
                for(i=0; i<strlen(ifilename); i++) 
                    if (ifilename[i] == ' ') 
                        ifilename[i] = '_';
                printf("%s\n", ifilename);
                if (savefiles == 1) {
                    if ((fptr = fopen(ifilename, "wb"))==NULL) {
                        printf("ERROR: Error writing file to disk!");
                    } else {
                        if (object == 1) {
                            fwrite(filedata->data + 16, 
                                    sizeof(BYTE), 
                                    filedata->size - 16, 
                                    fptr);
                        } else {
                            fwrite(filedata->data, 
                                    sizeof(BYTE), 
                                    filedata->size, 
                                    fptr);
                        }
                        fclose(fptr);
                    } // if we opened successfully
                } // if savefiles == 1
            } // if RealAttachment == 1
        } // if size>0
        p=p->next;
    } // while p!= null
}


#include "utility.c"
#include "vcal.c"
#include "vcard.c"
#include "vtask.c"



