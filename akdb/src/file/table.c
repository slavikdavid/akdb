﻿/**
@file table.c Provides functions for table abstraction
 */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#include "../file/table.h"


/**
 * @author Unknown
 * @brief Constructs a table parameter struct object
 * @param type parameter type
 * @param name parameter name
 * @return A pointer to the constructed AK_create_table_parameter object
 */
AK_create_table_parameter* AK_create_create_table_parameter(int type, char* name) {
    AK_PRO;
    AK_create_table_parameter* par = AK_malloc(sizeof (AK_create_table_parameter));
    par->type = type;
    strcpy(par->name, name);
    AK_EPI;
    return par;
}

/**
 * @author Unknown, updated by Josip Šušnjara (chained blocks support)
 * @brief Creates a table
 * @param tblName the name of the table
 * @param parameters table parameters array (each parameter contains name and type)
 * @param attribute_count the amount of attributes
 * @return No return value
 */
void AK_create_table(char* tblName, AK_create_table_parameter* parameters, int attribute_count) {
    AK_header* temp;
    AK_header t_header[attribute_count + 1];
    int startAddress;
    AK_PRO;
    
    for (int i = 0; i < attribute_count; i++) {
		    switch (parameters[i].type) {
		        case TYPE_INT:
		            temp = (AK_header*) AK_create_header(parameters[i].name, TYPE_INT, FREE_INT, FREE_CHAR, FREE_CHAR);
		            memcpy(t_header + i, temp, sizeof ( AK_header));
		            break;
		        case TYPE_VARCHAR:
		            temp = (AK_header*) AK_create_header(parameters[i].name, TYPE_VARCHAR, FREE_INT, FREE_CHAR, FREE_CHAR);
		            memcpy(t_header + i, temp, sizeof ( AK_header));
		            break;
		        case TYPE_FLOAT:
		            temp = (AK_header*) AK_create_header(parameters[i].name, TYPE_FLOAT, FREE_INT, FREE_CHAR, FREE_CHAR);
		            memcpy(t_header + i, temp, sizeof ( AK_header));
		            break;
		    }
		}
	temp = (AK_header*) AK_create_header("", TYPE_INTERNAL, FREE_INT, FREE_CHAR, FREE_CHAR);
	memcpy(t_header + attribute_count, temp, sizeof ( AK_header));
	startAddress = AK_initialize_new_segment(tblName, SEGMENT_TYPE_TABLE, t_header);

    if (startAddress != EXIT_ERROR) {
        printf("\nTABLE %s CREATED!\n", tblName);
    } else {
        printf("\nERROR: Failed to create table %s\n", tblName);
    }

    AK_EPI;
}

/**
 * @author Matija Novak, updated by Dino Laktašić
 * @brief  Temporary function that  creates table, and inserts an entry to the system_relation catalog
 * @param table table name
 * @param header AK_header of the new table
 * @param type_segment type of the new segment
 * @return No return value
 */
void AK_temp_create_table(char *table, AK_header *header, int type_segment) {
    AK_PRO;
	
	/* do we really need this???  Edited by Elvis Popovic*/
	/* AK_block *sys_block = (AK_block *) AK_malloc(sizeof (AK_block)); */
	AK_block *sys_block = AK_read_block(1);
	
    int startAddress = AK_initialize_new_segment(table, type_segment, header);

    int num = 8;
    const int objectID = 8;
    const int tableNameEntry = 9;
    const int startAddressEntry = 10;
    const int endAddressEntry = 11;

    //insert object_id
    AK_insert_entry(sys_block, TYPE_INT, &num, objectID);
    //insert table name
    AK_insert_entry(sys_block, TYPE_VARCHAR, table, tableNameEntry);
    //insert start address
    num = startAddress;
    AK_insert_entry(sys_block, TYPE_INT, &num, startAddressEntry);
    //insert end address
    num = startAddress + 19;
    AK_insert_entry(sys_block, TYPE_INT, &num, endAddressEntry);

    AK_write_block(sys_block);
    AK_free(sys_block);
    AK_EPI;
}

/**
 * @author Matija Šestak, updated by Josip Šušnjara (chained blocks support)
 * @brief  Functions that determines the number of attributes in the table
 * <ol>
 * <li>Read addresses of extents</li>
 * <li>If there is no extents in the table, return EXIT_WARNING</li>
 * <li>else read the first block</li>
 * <li>while  header tuple exists in the block, increment num_attr</li>
 * </ol>
 * @param  * tblName table name
 * @return number of attributes in the table
 */
int AK_num_attr(char * tblName) {
    int num_attr = 0;
    int i;
    AK_PRO;
    table_addresses *addresses = (table_addresses*) AK_get_table_addresses(tblName);
    if (addresses->address_from[0] == 0)
        num_attr = -2;
    else {
        AK_mem_block *temp_block = (AK_mem_block*) AK_get_block(addresses->address_from[0]);
		
		while(1){
			i = 0;
			while (strcmp(temp_block->block->header[i++].att_name, "\0") != 0) {
            	num_attr++;
        	}
        	if(temp_block->block->chained_with != NOT_CHAINED){
        		temp_block = (AK_mem_block*) AK_get_block(temp_block->block->chained_with);
        	}
        	else{
        		break;
        	}
		}
    }
    AK_free(addresses);
    AK_EPI;
    return num_attr;
}

/**
 * @author Matija Šestak, updated by Josip Šušnjara (chained blocks support)
 * @brief  Function that determines the number of rows in the table
 * <ol>
 * <li>Read addresses of extents</li>
 * <li>If there is no extents in the table, return EXIT_WARNING</li>
 * <li>For each extent from table</li>
 * <li>For each block in the extent</li>
 * <li>Get a block</li>
 * <li>Exit if there is no records in block</li>
 * <li>Count tuples in block</li>
 * <li>Return the number of tuples divided by number of attributes</li>
 * </ol>
 * @param *tableName table name
 * @return number of rows in the table
 */
int AK_get_num_records(char *tblName) {
    int num_rec = 0;
    int blocks_per_row; //how many chained blocks are needed to store one entry of the table
    int i = 0, j, k;
    int num_head;
    AK_PRO;
    table_addresses *addresses = AK_get_table_addresses(tblName);
    blocks_per_row = (AK_num_attr(tblName) - 1) / MAX_ATTRIBUTES + 1;
    if (addresses->address_from[0] == 0){
        AK_EPI;
        return EXIT_WARNING;
    }
    AK_mem_block *temp = AK_get_block(addresses->address_from[0]);
    
    while (addresses->address_from[i] != 0) {
        for (j = addresses->address_from[i]; j < addresses->address_to[i]; j += blocks_per_row) {
            temp = AK_get_block(j);
            if (temp->block->last_tuple_dict_id == 0)
                break;
            for (k = 0; k < DATA_BLOCK_SIZE; k++) {
                if (temp->block->tuple_dict[k].size > 0) {
                    num_rec++;
                }
            }
        }
        i++;
    }

    AK_free(addresses);
    num_head = AK_num_attr(tblName);
    if(num_head > MAX_ATTRIBUTES){
    	num_head = MAX_ATTRIBUTES;
    }
    AK_EPI;
    return num_rec / num_head;
}

/**
 * @author Matija Šestak, updated by Josip Šušnjara (chained blocks support)
 * @brief  Function that fetches the table header
 * <ol>
 * <li>Read addresses of extents</li>
 * <li>If there is no extents in the table, return 0</li>
 * <li>else read the first block</li>
 * <li>allocate array</li>
 * <li>copy table header to the array</li>
 * </ol>
 * @param  *tblName table name
 * @result array of table header
 */
AK_header *AK_get_header(char *tblName) {
	int num_attr;
	int current_attr;
    AK_PRO;
    table_addresses *addresses = AK_get_table_addresses(tblName);
    if (addresses->address_from[0] == 0){
        AK_EPI;
        return EXIT_WARNING + 2;
    }
    AK_mem_block *temp = AK_get_block(addresses->address_from[0]);

    num_attr = AK_num_attr(tblName);
    
    // Is calloc the right choice here? The memory gets overridden immediately anyway
    //AK_header *head = (AK_header*) AK_calloc(num_attr, sizeof (AK_header));
    AK_header *head = AK_malloc(sizeof(AK_header) * num_attr);
    current_attr = 0;
    while(1){
        for (int i = 0; i < MAX_ATTRIBUTES && current_attr < num_attr; i++){
            if (strcmp(temp->block->header[i].att_name, "\0") == 0){
                break;
            }
            head[current_attr] = temp->block->header[i];
            current_attr++;
        }
        if(temp->block->chained_with != NOT_CHAINED){
			temp = AK_get_block(temp->block->chained_with);
		}
		else{
			break;
		}
	}

	AK_free(addresses);
    AK_EPI;
    return head;
}

/**
 * @author Matija Šestak
 * @brief  Function that fetches attribute name for some zero-based index
 * @param *tblName table name
 * @param index zero-based index
 * @return attribute name
 */
char *AK_get_attr_name(char *tblName, int index) {
    AK_PRO;
    int num_attr = AK_num_attr(tblName);
    if (index >= num_attr){
        AK_EPI;
        return NULL;
    }
    else {
        AK_header *header = AK_get_header(tblName);
        AK_EPI;
        return (header + index)->att_name;
    }
    AK_EPI;
}

/**
 * @author Matija Šestak.
 * @brief  Function that fetches zero-based index for atrribute
 * @param  *tblName table name
 * @param *attrName attribute name
 * @return zero-based index
 */
int AK_get_attr_index(char *tblName, char *attrName) {
    AK_PRO;
    if (tblName == NULL || attrName == NULL){
        AK_EPI;
        return EXIT_WARNING;
    }
    int num_attr = AK_num_attr(tblName);
    AK_header *header = AK_get_header(tblName);
    for(int index = 0; index < num_attr; index++) {
        if (strcmp(attrName, (header + index)->att_name) == 0) {
			AK_free(header);
            AK_EPI;
            return index;
        }
    }
	AK_free(header);
    AK_EPI;
    return EXIT_WARNING;
}

/**
 * @author Matija Šestak, updated by Josip Šušnjara (chained blocks support)
 * @brief  Function that fetches all values in some column and put on the list
 * @param num zero-based column index
 * @param  *tblName table name
 * @return column values list
 */
struct list_node *AK_get_column(int num, char *tblName) {
	int blocks_per_row;
	int increment;
    AK_PRO;
    int num_attr = AK_num_attr(tblName);
    if (num >= num_attr || num < 0){
        AK_EPI;
        return NULL;
    }
    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof (struct list_node));

    AK_Init_L3(&row_root);

    table_addresses *addresses = (table_addresses*) AK_get_table_addresses(tblName);
    int i, j, k;
    char data[ MAX_VARCHAR_LENGTH ];
    
    blocks_per_row = (num_attr - 1) / MAX_ATTRIBUTES + 1;
    if (blocks_per_row > 1 && (num / MAX_ATTRIBUTES) < blocks_per_row - 1)
    	increment = MAX_ATTRIBUTES;
    else if (blocks_per_row > 1)
    	increment = num_attr % MAX_ATTRIBUTES;
    else
    	increment = num_attr;

    i = 0;
    while (addresses->address_from[i] != 0) {
        for (j = addresses->address_from[i]; j < addresses->address_to[i]; j += blocks_per_row) {
            AK_mem_block *temp = (AK_mem_block*) AK_get_block(j);
            if (temp->block->last_tuple_dict_id == 0) break;
            
            while(num >= MAX_ATTRIBUTES){
                temp = (AK_mem_block*) AK_get_block(++j);
                num -= MAX_ATTRIBUTES;
            }
            
            for (k = num; k < DATA_BLOCK_SIZE; k += increment) {
                if (temp->block->tuple_dict[k].type != FREE_INT) {
                    int type = temp->block->tuple_dict[k].type;
                    int size = temp->block->tuple_dict[k].size;
                    int address = temp->block->tuple_dict[k].address;
                    memcpy(data, &(temp->block->data[address]), size);
                    data[ size ] = '\0';
                    AK_InsertAtEnd_L3(type, data, size, row_root);
                }
            }
        }
        i++;
    }
    AK_EPI;
    return row_root;
}

/**
 * @author Markus Schatten, Matija Šestak.
 * @brief  Function that fetches all values in some row and put on the list
 * @param num zero-based row index
 * @param  * tblName table name
 * @return row values list
 */
struct list_node *AK_get_row(int num, char * tblName) {
    AK_PRO;
    table_addresses *addresses = AK_get_table_addresses(tblName);
    struct list_node *row_root = (struct list_node *) AK_calloc(1, sizeof (struct list_node));
    AK_Init_L3(&row_root);

    int num_attr = AK_num_attr(tblName);
    int i, j, k, l, counter;
    i = 0;

    char data[MAX_VARCHAR_LENGTH];
    counter = -1;
    while (addresses->address_from[i] != 0) {
        for (j = addresses->address_from[i]; j < addresses->address_to[i]; j++) {
            AK_mem_block *temp = (AK_mem_block*) AK_get_block(j);
            if (temp->block->last_tuple_dict_id == 0)
                break;
            for (k = 0; k < DATA_BLOCK_SIZE; k += num_attr) {
                if (temp->block->tuple_dict[k].size > 0)
                    counter++;
                if (counter == num) {
                    for (l = 0; l < num_attr; l++) {
                        int type = temp->block->tuple_dict[k + l].type;
                        int size = temp->block->tuple_dict[k + l].size;
                        int address = temp->block->tuple_dict[k + l].address;
                        memcpy(data, &(temp->block->data[address]), size);
                        data[size] = '\0';
                        AK_InsertAtEnd_L3(type, data, size, row_root);
                    }
                    AK_free(addresses);
                    AK_EPI;
                    return row_root;
                }
            }
        }
        i++;
    }
    AK_free(addresses);
	AK_DeleteAll_L3(&row_root);
	AK_free(row_root);
    AK_EPI;
    return NULL;
}

/**
 * @author Barbara Tatai, updated by Josip Šušnjara (chained blocks support)
 * @brief Function that finds the tuple in memory
 * @param row zero-based row index
 * @param column zero-based column index
 * @param num_attr the number of attributes in the table
 * @param addresses table addresses
 * @param row_root the root node of the list of rows
 * @return a pointer to a list_node representing the element tuple
 */
struct list_node *AK_find_tuple(int row, int column, int num_attr, table_addresses *addresses, struct list_node *row_root) {
    int i, j, k;
    int counter;
    int blocks_per_row;
    int increment; //if there are e.g. 23 attributes, and MAX_ATTRIBUTES is 10, then 1st block takes 10 atts, 2nd also 10, and 3rd takes 3 atts
    char data[MAX_VARCHAR_LENGTH];
	AK_PRO;

    i = 0;
    counter = -1;
    blocks_per_row = (num_attr - 1) / MAX_ATTRIBUTES + 1;
    
    if (blocks_per_row > 1 && (column / MAX_ATTRIBUTES) < blocks_per_row - 1)
    	increment = MAX_ATTRIBUTES;
    else if (blocks_per_row > 1)
    	increment = num_attr % MAX_ATTRIBUTES;
    else
    	increment = num_attr;
    	
    while (addresses->address_from[ i ] != 0) {
        for (j = addresses->address_from[ i ]; j < addresses->address_to[ i ]; j += blocks_per_row) {
            AK_mem_block *temp = (AK_mem_block*) AK_get_block(j);
            if (temp->block->last_tuple_dict_id == 0) break;
            while(column >= MAX_ATTRIBUTES){//e.g. 13th column in table is 3rd column in 2nd block of the table
                temp = (AK_mem_block*) AK_get_block(++j);
                column -= MAX_ATTRIBUTES;
            }
            for (k = 0; k < DATA_BLOCK_SIZE; k += increment) {
                if (temp->block->tuple_dict[k].size > 0)
                    counter++;
                if (counter == row) {
                	
					struct list_node *next;
                    int type = temp->block->tuple_dict[ k + column ].type;
                    int size = temp->block->tuple_dict[ k + column ].size;
                    int address = temp->block->tuple_dict[ k + column ].address;
                    memcpy(data, &(temp->block->data[address]), size);
                    data[ size ] = '\0';
                    AK_InsertAtEnd_L3(type, data, size, row_root);
                    AK_free(addresses);
					next = AK_First_L2(row_root); //store next
                    AK_free(row_root);
					//returns next in row_root leaving base of the list allocated, so we made some corrections
                    //return (struct list_node *) AK_First_L2(row_root);
					return next; //returns next
                }
            }
        }
        i++;
    }
    return NULL;
}


/**
 * @author Matija Šestak
 * @brief Function that fetches a value in some row and column
 * @param row zero-based row index
 * @param column zero-based column index
 * @param *tblName table name
 * @return value in the list
 */
struct list_node *AK_get_tuple(int row, int column, char *tblName) {
    AK_PRO;
    int num_rows = AK_get_num_records(tblName);
    int num_attr = AK_num_attr(tblName);

    if (row >= num_rows || column >= num_attr) {
        AK_EPI;
        return NULL;
    }

    table_addresses *addresses = (table_addresses*) AK_get_table_addresses(tblName);

    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof (struct list_node));
    AK_Init_L3(&row_root);

    struct list_node* tupleFound = AK_find_tuple(row, column, num_attr, addresses, row_root);
    if(tupleFound == NULL) {
        AK_free(addresses);
        AK_DeleteAll_L3(&row_root);
        AK_free(row_root);
    }
    AK_EPI;
    return tupleFound;
}

/**
 * @author Matija Šestak.
 * @brief  Function that converts tuple value to string
 * @param *tuple tuple in the list
 * @return tuple value as a string
 */
char * AK_tuple_to_string(struct list_node *tuple) {
    int temp_int;
    double temp_float;
    char temp_char[ MAX_VARCHAR_LENGTH ];
    AK_PRO;
    char *buff = (char*) AK_malloc(MAX_VARCHAR_LENGTH);

    //assert(tuple->type);

    switch (tuple->type) 
	{
        case TYPE_INT:
            memcpy(&temp_int, tuple->data, tuple->size);
            sprintf(buff, "%d", temp_int);
            AK_EPI;
            return buff;
            break;
        case TYPE_FLOAT:
            memcpy(&temp_float, tuple->data, tuple->size);
            sprintf(buff, "%f", temp_float);
            AK_EPI;
            return buff;
            break;
        case TYPE_VARCHAR:
            memcpy(temp_char, tuple->data, tuple->size);
            temp_char[ tuple->size ] = '\0';
            sprintf(buff, "%s", temp_char);
            AK_EPI;
            return buff;
            break;
    }
	AK_free(buff);
    AK_EPI;
    return NULL;
}

/**
 * @author Dino Laktašić.
 * @brief Function that prints row spacer
 * @param col_len[] max lengths for each attribute cell
 * @param length total table width
 * @return printed row spacer
 */
void AK_print_row_spacer(int col_len[], int length) {
    int i, j, col, temp;
    AK_PRO;
    j = col = temp = 0;

    for (i = 0; i < length; i++) {
        if (!i || i == temp + j) {
            j += TBL_BOX_OFFSET + 1;
            temp += col_len[col++] + 1;
            printf("+");
        } else {
            printf("-");
        }
    }
    AK_EPI;
}

/**
 * @author Dino Laktašić
 * @brief  Function that prints table row
 * @param col_len[] array of max lengths for each attribute
 * @param *row  list with row elements
 * @return No return value
 */
void AK_print_row(int col_len[], struct list_node *row) {
    AK_PRO;
    struct list_node *el = (struct list_node *) AK_First_L2(row);

    int i = 0;
    void *data = (void *) AK_calloc(MAX_VARCHAR_LENGTH, sizeof (void));

    printf("\n|");
    while (el != NULL) {
        memset(data, 0, MAX_VARCHAR_LENGTH);
        switch (el->type) {
            case FREE_CHAR:
                //case FREE_INT:
                printf(" %-*s|", col_len[i] + TBL_BOX_OFFSET, "null");
                break;
            case TYPE_INT:
                memcpy(data, el->data, sizeof (int));
                printf("%*i |", col_len[i] + TBL_BOX_OFFSET, *((int *) data));
                break;
            case TYPE_FLOAT:
                memcpy(data, el->data, sizeof (float));
                printf("%*.3f |", col_len[i] + TBL_BOX_OFFSET, *((float *) data));
                break;
                //Currently this header types are handled like TYPE_VARCHAR (print as string left aligned)
            case TYPE_VARCHAR:
            default:
                memcpy(data, el->data, el->size);
                printf(" %-*s|", col_len[i] + TBL_BOX_OFFSET, (const char *) data);
                break;
        }
        el = el->next;
        i++;
    }
    printf("\n");
    AK_free(data);
    AK_EPI;
}

/**
 * @author Jurica Hlevnjak
 * @brief Function that examines whether there is a table with the name "tblName" in the system catalog (AK_relation)
 * @param tblName table name
 * @return returns 1 if table exist or returns 0 if table does not exist
 */
int AK_table_exist(char *tblName) {
    AK_PRO;
    char *sys_table = "AK_relation";
    int num_rows = AK_get_num_records(sys_table);
    int a;
    int exist = 0;
	struct list_node *el;
	
    for (a = 0; a < num_rows; a++) 
	{
        el = AK_get_tuple(a, 1, sys_table);
        if (!strcmp(tblName, el->data)) 
		{
            exist = 1;
			AK_DeleteAll_L3(&el);
			AK_free(el);
            break;
        }
		AK_DeleteAll_L3(&el);
		AK_free(el);
    }
    AK_EPI;
    return exist;
}

/**
 * @author Dino Laktašić and Mislav Čakarić (replaced old print table function by new one), updated by Josip Šušnjara (chained blocks support)
 * @brief  Function for printing table
 * @param *tblName table name
 * @return No return value
 */
void AK_print_table(char *tblName) {
	int i, j, k, l;
    int blocks_per_row;
    
    AK_PRO;
    table_addresses *addresses = (table_addresses*) AK_get_table_addresses(tblName);
    //  || (AK_table_exist(tblName) == 0)
    if ((addresses->address_from[0] == 0) || (AK_table_exist(tblName) == 0)) {
        printf("Table %s does not exist!\n", tblName);
        AK_free(addresses);
    } else {
        AK_header *head = AK_get_header(tblName);

        int num_attr = AK_num_attr(tblName);
        int num_rows = AK_get_num_records(tblName);
        int len[num_attr]; //max length for each attribute in row
        int length = 0; //length of spacer
        clock_t t;

        //store lengths of header attributes
        for (i = 0; i < num_attr; i++) {
            len[i] = strlen((head + i)->att_name);
        }
		
        //for each header attribute iterate through all table rows and check if
        //there is longer element than previously longest and store it in array
        for (i = 0; i < num_attr; i++) {
            for (j = 0; j < num_rows; j++) 
			{
                struct list_node *el = AK_get_tuple(j, i, tblName);
                switch (el->type) 
				{
                    case TYPE_INT:
                        length = AK_chars_num_from_number(*((int *) (el)->data), 10);
                        if (len[i] < length) 
						{
                            len[i] = length;
                        }
                        break;
                    case TYPE_FLOAT:
                        length = AK_chars_num_from_number(*((float *) (el)->data), 10);
                        if (len[i] < length) 
						{
                            len[i] = length;
                        }
                        break;
                    case TYPE_VARCHAR:
                    default:
                        if (len[i] < el->size) 
						{
                            len[i] = el->size;
                        }
                        break;
                }
				//we don't need this linked list anymore (starting from tupple first to the end
				//see comment above in function AK_get_tuple - Elvis Popovic
				AK_DeleteAll_L3(&el);
				AK_free(el);
            }
        }
        //num_attr is number of char | + space in printf
        //set offset to change the box size
        length = 0;
        for (i = 0; i < num_attr; length += len[i++]);
        length += num_attr * TBL_BOX_OFFSET + 2 * num_attr + 1;

        //start measuring time
        t = clock();

        printf("Table: %s\n", tblName);

        if (num_attr <= 0 || num_rows <= 0) {
            printf("Table is empty.\n");
        } else {
            //print table header
            AK_print_row_spacer(len, length);
            printf("\n|");

            for (i = 0; i < num_attr; i++) {
                //print attributes center aligned inside box
                k = (len[i] - (int) strlen((head + i)->att_name) + TBL_BOX_OFFSET + 1);
                if (k % 2 == 0) {
                    k /= 2;
                    printf("%-*s%-*s|", k, " ", k + (int) strlen((head + i)->att_name), (head + i)->att_name);
                } else {
                    k /= 2;
                    printf("%-*s%-*s|", k, " ", k + (int) strlen((head + i)->att_name) + 1, (head + i)->att_name);
                }

                //print attributes left aligned inside box
                //printf(" %-*s|", len[i] + MAX_TABLE_BOX_OFFSET, (head + i)->att_name);
            }
			AK_free(head);
            printf("\n");
            AK_print_row_spacer(len, length);

            struct list_node *row_root = (struct list_node *) AK_malloc(sizeof (struct list_node));
            AK_Init_L3(&row_root);

            i = 0;
            int type, size, address;
            
            int blocks_per_row = (num_attr - 1) / MAX_ATTRIBUTES + 1;

            while (addresses->address_from[i] != 0) {
                for (j = addresses->address_from[i]; j < addresses->address_to[i]; j += blocks_per_row) {
                	AK_mem_block *temp[blocks_per_row];
                    temp[0] = (AK_mem_block*) AK_get_block(j);
                    
                    for(int m = 1; temp[m - 1]->block->chained_with != NOT_CHAINED; m++){
                    	temp[m] = (AK_mem_block*) AK_get_block(temp[m - 1]->block->chained_with);
                    }
                    if (temp[0]->block->last_tuple_dict_id == 0)
                        break;
                    int increment = num_attr;
                    if(num_attr > MAX_ATTRIBUTES)
                    	increment = MAX_ATTRIBUTES;
                    for (k = 0; k * increment < DATA_BLOCK_SIZE; k++) {
                    	
		                if(num_attr > MAX_ATTRIBUTES)
		                	increment = MAX_ATTRIBUTES;
		                else
		                	increment = num_attr;
                        if (temp[0]->block->tuple_dict[k * increment].size > 0) {
                            for (l = 0; l < num_attr; l++) {
                            	int block_index = 0; //defines in which block of chained blocks is a cell we are looking for
                            	block_index = l / MAX_ATTRIBUTES;
                            	if (blocks_per_row > 1 && block_index < blocks_per_row - 1)
									increment = MAX_ATTRIBUTES;
								else if (blocks_per_row > 1)
									increment = num_attr % MAX_ATTRIBUTES;
								else
									increment = num_attr;
                                type = temp[block_index]->block->tuple_dict[k * increment + (l % MAX_ATTRIBUTES)].type;
                                size = temp[block_index]->block->tuple_dict[k * increment + (l % MAX_ATTRIBUTES)].size;
                                address = temp[block_index]->block->tuple_dict[k * increment + (l % MAX_ATTRIBUTES)].address;
                                AK_InsertAtEnd_L3(type, &temp[block_index]->block->data[address], size, row_root);
                            }
                            AK_print_row(len, row_root);
                            AK_print_row_spacer(len, length);
                            AK_DeleteAll_L3(&row_root);
                        }
                    }
                }
                i++;
            }

            printf("\n");
            t = clock() - t;

            if ((((double) t) / CLOCKS_PER_SEC) < 0.1) {
                printf("%i rows found, duration: %f μs\n", num_rows, ((double) t) / CLOCKS_PER_SEC * 1000);
            } else {
                printf("%i rows found, duration: %f s\n", num_rows, ((double) t) / CLOCKS_PER_SEC);
            }
        AK_free(row_root);
        }

AK_free(addresses);

    }
    AK_EPI;
}

/**
 * @author Dino Laktašić.
 * @brief Function that prints row spacer
 * update by Luka Rajcevic
 * @param col_len[] max lengths for each attribute cell
 * @param length total table width
 * @return printed row spacer
 */
void AK_print_row_spacer_to_file(int col_len[], int length) {

	char* FILEPATH = "table_test.txt";
	FILE *fp;
    
	AK_PRO;
    fp = fopen(FILEPATH, "a");
    int i, j, col, temp;

    j = col = temp = 0;

    for (i = 0; i < length; i++) {
        if (!i || i == temp + j) {
            j += TBL_BOX_OFFSET + 1;
            temp += col_len[col++] + 1;
            fprintf(fp, "+");
        } else {
            fprintf(fp, "-");
        }
    }

    fclose(fp);
    AK_EPI;
}
/**
 * @author Leon Palaić
 * @brief  Function that returns the value of an attribute from the row
 * @param column index of column atribute
 * @param *row  list with row elements
 * @return atribute data
 */
char *get_row_attr_data(int column, struct list_node *node){
    
    struct list_node *nodee = node;
    int i=0;
    while(nodee->next){
        
        if(i == column)
            return nodee->next->data;
        else{
            i++;
            nodee = nodee->next;
            }
        }
}


/**
 * @author Dino Laktašić
 * @brief  Function that prints the table row
 * update by Luka Rajcevic
 * @param col_len[] array of max lengths for each attribute
 * @param *row  list with row elements
 * @return No return value
 */
void AK_print_row_to_file(int col_len[], struct list_node * row) {

	char* FILEPATH = "table_test.txt";
	FILE *fp;
    
	AK_PRO;
    fp = fopen(FILEPATH, "a");
    struct list_node *el = (struct list_node *) AK_First_L2(row);

    int i = 0;
    void *data = (void *) AK_calloc(MAX_VARCHAR_LENGTH, sizeof (void));

    fprintf(fp, "\n|");
    while (el != NULL) {
        memset(data, 0, MAX_VARCHAR_LENGTH);
        switch (el->type) {
            case FREE_CHAR:
                //case FREE_INT:
                fprintf(fp, " %-*s|", col_len[i] + TBL_BOX_OFFSET, "null");
                break;
            case TYPE_INT:
                memcpy(data, el->data, sizeof (int));
                fprintf(fp, "%*i |", col_len[i] + TBL_BOX_OFFSET, *((int *) data));
                break;
            case TYPE_FLOAT:
                memcpy(data, el->data, sizeof (float));
                fprintf(fp, "%*.3f |", col_len[i] + TBL_BOX_OFFSET, *((float *) data));
                break;
            case TYPE_VARCHAR:
            default:
                memcpy(data, el->data, el->size);
                fprintf(fp, " %-*s|", col_len[i] + TBL_BOX_OFFSET, (const char *) data);
                break;
        }
        el = el->next;
        i++;
    }
    fprintf(fp, "\n");
    fclose(fp);
    AK_free(data);
    AK_EPI;
}

/**
 * @author Dino Laktašić and Mislav Čakarić (replaced old print table function by new one), updated by Josip Šušnjara (chained blocks support)
 * update by Luka Rajcevic
 * @brief  Function that prints a table
 * @param *tblName table name
 * @return No return value
 * update by Anto Tomaš (corrected the AK_DeleteAll_L3 function)
 */
void AK_print_table_to_file(char *tblName) {

	char* FILEPATH = "table_test.txt";
	FILE *fp;
	int blocks_per_row;
    
	AK_PRO;
    fp = fopen(FILEPATH, "a");
    table_addresses *addresses = (table_addresses*) AK_get_table_addresses(tblName);
    if (addresses->address_from[0] == 0) {
        fprintf(fp, "Table %s does not exist!\n", tblName);

    } else {
        AK_header *head = AK_get_header(tblName);

        int i, j, k, l;
        int num_attr = AK_num_attr(tblName);
        int num_rows = AK_get_num_records(tblName);
        int len[num_attr]; //max length for each attribute in row
        int length = 0; //length of spacer

        //store lengths of header attributes
        for (i = 0; i < num_attr; i++) {
            len[i] = strlen((head + i)->att_name);
        }

        //for each header attribute iterate through all table rows and check if
        //there is longer element than previously longest and store it in array
        for (i = 0; i < num_attr; i++) {
            for (j = 0; j < num_rows; j++) {
                struct list_node *el = AK_get_tuple(j, i, tblName);
                switch (el->type) {
                    case TYPE_INT:
                        length = AK_chars_num_from_number(*((int *) (el)->data), 10);
                        if (len[i] < length) {
                            len[i] = length;
                        }
                        break;
                    case TYPE_FLOAT:
                        length = AK_chars_num_from_number(*((float *) (el)->data), 10);
                        if (len[i] < length) {
                            len[i] = length;
                        }
                        break;
                    case TYPE_VARCHAR:
                    default:
                        if (len[i] < el->size) {
                            len[i] = el->size;
                        }
                        break;
                }
            }
        }
        //num_attr is number of char | + space in printf
        //set offset to change the box size
        length = 0;
        for (i = 0; i < num_attr; length += len[i++]);
        length += num_attr * TBL_BOX_OFFSET + 2 * num_attr + 1;

        fprintf(fp, "Table: %s\n", tblName);

        if (num_attr <= 0 || num_rows <= 0) {
            fprintf(fp, "Table is empty.\n");
        } else {
            //print table header
            fclose(fp);
            AK_print_row_spacer_to_file(len, length);
            fp = fopen(FILEPATH, "a");
            fprintf(fp, "\n|");

            for (i = 0; i < num_attr; i++) {
                //print attributes center aligned inside box
                k = (len[i] - (int) strlen((head + i)->att_name) + TBL_BOX_OFFSET + 1);
                if (k % 2 == 0) {
                    k /= 2;
                    fprintf(fp, "%-*s%-*s|", k, " ", k + (int) strlen((head + i)->att_name), (head + i)->att_name);
                } else {
                    k /= 2;
                    fprintf(fp, "%-*s%-*s|", k, " ", k + (int) strlen((head + i)->att_name) + 1, (head + i)->att_name);
                }

                //print attributes left aligned inside box
                //printf(" %-*s|", len[i] + MAX_TABLE_BOX_OFFSET, (head + i)->att_name);
            }
            fprintf(fp, "\n");
            fclose(fp);
            AK_print_row_spacer_to_file(len, length);
            struct list_node *row_root = (struct list_node *) AK_malloc(sizeof (struct list_node));
            AK_Init_L3(&row_root);

            i = 0;
            int type, size, address;
            
            blocks_per_row = (num_attr - 1) / MAX_ATTRIBUTES + 1;

            while (addresses->address_from[i] != 0) {
                for (j = addresses->address_from[i]; j < addresses->address_to[i]; j += blocks_per_row) {
                    AK_mem_block *temp[blocks_per_row];
                    temp[0] = (AK_mem_block*) AK_get_block(j);
                    for(int m = 1; temp[m - 1]->block->chained_with != NOT_CHAINED; m++){
                    	temp[m] = (AK_mem_block*) AK_get_block(temp[m - 1]->block->chained_with);
                    }
                    if (temp[0]->block->last_tuple_dict_id == 0)
                        break;
                    int increment = num_attr;
                    if(num_attr > MAX_ATTRIBUTES)
                    	increment = MAX_ATTRIBUTES;
                    for (k = 0; k * increment < DATA_BLOCK_SIZE; k++) {
                    	if(num_attr > MAX_ATTRIBUTES)
		                	increment = MAX_ATTRIBUTES;
		                else
		                	increment = num_attr;
                        if (temp[0]->block->tuple_dict[k * increment].size > 0) {
                            for (l = 0; l < num_attr; l++) {
                            	int block_index = 0; //defines in which block of chained blocks is a cell we are looking for
                            	block_index = l / MAX_ATTRIBUTES;
                            	if (blocks_per_row > 1 && block_index < blocks_per_row - 1)
									increment = MAX_ATTRIBUTES;
								else if (blocks_per_row > 1)
									increment = num_attr % MAX_ATTRIBUTES;
								else
									increment = num_attr;
                                type = temp[block_index]->block->tuple_dict[k * increment + (l % MAX_ATTRIBUTES)].type;
                                size = temp[block_index]->block->tuple_dict[k * increment + (l % MAX_ATTRIBUTES)].size;
                                address = temp[block_index]->block->tuple_dict[k * increment + (l % MAX_ATTRIBUTES)].address;
                                AK_InsertAtEnd_L3(type, &(temp[block_index]->block->data[address]), size, row_root);
                            }
                            AK_print_row_to_file(len, row_root);
                            AK_print_row_spacer_to_file(len, length);
                            AK_DeleteAll_L3(&row_root);
                        }
                    }
                }
                i++;
            }
            fp = fopen(FILEPATH, "a");
            fprintf(fp, "\n");
        }
    }
    fclose(fp);
    AK_EPI;
}

/**
 * @author Matija Šestak.
 * @brief  Function that checks whether the table is empty
 * @param *tblName table name
 * @return true/false
 */
int AK_table_empty(char *tblName) {
    AK_PRO;
    table_addresses *addresses = (table_addresses*) AK_get_table_addresses(tblName);
    AK_mem_block *temp = (AK_mem_block*) AK_get_block(addresses->address_from[0]);
    AK_free(addresses);
    AK_EPI;
    return (temp->block->last_tuple_dict_id == 0) ? 1 : 0;
}

/**
 * @author Dejan Frankovic
 * @brief  Function that fetches an obj_id of named table from AK_relation system table
 * @param *table table name
 * @return obj_id of the table or EXIT_ERROR if there is no table with that name
 */
int AK_get_table_obj_id(char *table) {
    int i = 0, table_id = -1;
    struct list_node *row;

    i = 0;
    AK_PRO;
    while ((row = AK_get_row(i, "AK_relation")) != NULL) {
        if (strcmp(row->next->next->data, table) == 0) {
            memcpy(&table_id, row->next->data, sizeof (int));
            break;
        }
        i++;
    }
    if (table_id == -1){
        AK_EPI;
        return EXIT_ERROR;
    }
    AK_EPI;
    return table_id;
}

/**
 * @author Dino Laktašić, abstracted from difference.c for use in difference.c, intersect.c and union.c by Tomislav Mikulček
 * @brief  Function that checks if tables have the same relation schema
 * @param tbl1_temp_block first cache block of the first table
 * @param tbl2_temp_block first cache block of the second table
 * @param operator_name the name of operator, used for displaying error message
 * @return if success returns num of attributes in schema, else returns EXIT_ERROR
 */
int AK_check_tables_scheme(AK_mem_block *tbl1_temp_block, AK_mem_block *tbl2_temp_block, char *operator_name) {
    int i;
    int num_att1 = 0;
    int num_att2 = 0;
    AK_PRO;
    for (i = 0; i < MAX_ATTRIBUTES; i++) {
        if (strcmp(tbl1_temp_block->block->header[i].att_name, "\0") != 0) {
            num_att1++;
        } else {
            break;
        }

        if (strcmp(tbl2_temp_block->block->header[i].att_name, "\0") != 0) {
            num_att2++;
        } else {
            break;
        }

        if (strcmp(tbl1_temp_block->block->header[i].att_name, tbl2_temp_block->block->header[i].att_name) != 0) {
            printf("%s ERROR: Relation shemas are not the same! \n", operator_name);
	    AK_EPI;
            return EXIT_ERROR;
        }

        if (tbl1_temp_block->block->header[i].type != tbl2_temp_block->block->header[i].type) {
            printf("%s ERROR: Attributes are not of the same type!", operator_name);
            AK_EPI;
            return EXIT_ERROR;
        }
    }

    if (num_att1 != num_att2) {
        printf("%s ERROR: Not same number of the attributes! \n", operator_name);
        AK_EPI;
        return EXIT_ERROR;
    }
    AK_EPI;
    return num_att1;
}

/**
 * @author Mislav Čakarić edited by Ljubo Barać
 * @brief Function for renaming table and/or attribute in table (moved from rename.c)
 * @param old_table_name old name of the table
 * @param new_table_name new name of the table
 * @param old_attr name of the attribute to rename
 * @param new_attr new name for the attribute to rename
 * @return EXIT_ERROR or EXIT_SUCCESS
 */
int AK_rename(char *old_table_name, char *old_attr, char *new_table_name, char *new_attr) {
    AK_PRO;
    table_addresses *adresses = (table_addresses *) AK_get_table_addresses(old_table_name);
    int tab_addresses[MAX_NUM_OF_BLOCKS];
    int num_extents = 0, num_blocks = 0;
    register int i, j;
    AK_mem_block *mem_block;

    if (strcmp(old_attr, new_attr) != 0) {
        //SEARCH FOR ALL BLOCKS IN SEGMENT
        i = 0;
        while (adresses->address_from[i]) {
            for (j = adresses->address_from[i]; j <= adresses->address_to[i]; j++) {
                tab_addresses[num_blocks] = j;
                num_blocks++;
            }
            num_extents++;
            i++;
        }

        AK_header newHeader[MAX_ATTRIBUTES];
        mem_block = (AK_mem_block *) AK_get_block(tab_addresses[0]);
        memcpy(&newHeader, mem_block->block->header, sizeof (mem_block->block->header));

        for (i = 0; i < MAX_ATTRIBUTES; i++) {
            if (strcmp(newHeader[i].att_name, old_attr) == 0) {
                AK_dbg_messg(HIGH, REL_OP, "AK_rename: the attribute names are the same at position %d!\n", i);
                memset(&newHeader[i].att_name, 0, MAX_ATT_NAME);
                memcpy(&newHeader[i].att_name, new_attr, strlen(new_attr));
                break;
            } else if (strcmp(newHeader[i].att_name, "\0") == 0) { //if there is no more attributes
                AK_dbg_messg(MIDDLE, REL_OP, "AK_rename: ERROR: atribute: %s does not exist in this table\n", old_attr);
                AK_EPI;
                return (EXIT_ERROR);
            }
        }

        //replacing old headers with new ones
        for (i = 0; i < num_blocks; i++) {
            mem_block = AK_get_block(tab_addresses[i]);
            memcpy(&mem_block->block->header, newHeader, sizeof (AK_header) * MAX_ATTRIBUTES);
            AK_mem_block_modify(mem_block, BLOCK_DIRTY);
        }
    }

    if (strcmp(old_table_name, new_table_name) != 0) {//new name is different than old, and old needs to be replaced
        struct list_node *expr;
        expr = 0;
        AK_selection_op_rename(old_table_name, new_table_name, expr);
        int i = 0;

  for (;adresses->address_from[i] != 0; ++i)
    {
      if (AK_delete_extent(adresses->address_from[i], adresses->address_to[i]) == EXIT_ERROR)
	{
	  AK_EPI;
	  return EXIT_ERROR;
        }
    }
	
  struct list_node* row_root = (struct list_node*) AK_malloc(sizeof(struct list_node));
  AK_Init_L3(&row_root);
  
  char *system_table;
  system_table = "AK_relation";
   
  
  AK_DeleteAll_L3(&row_root);
  AK_Update_Existing_Element(TYPE_VARCHAR, old_table_name, system_table, "name", row_root);
  AK_delete_row(row_root);
  AK_free(row_root);
    }
    AK_EPI;
    return EXIT_SUCCESS;
}






/**
 * @author Matija Šestak
 * @brief Function for testing table abstraction
 * @return TestResult containing information on the amount of failed/passed tests

    @update by Ana-Marija Balen - added getRow function to the test
    @update by Barbara Tatai - fixed SIGSEGV (caused by storing char pointers into integers), fixed successful/failed counter
 */
TestResult AK_table_test() {
	
	
	/* ------------------------------------------------------------------------------------------------------
		The commented part was intended for chained blocks implementation testing
		(if a table has more than MAX_ATTRIBUTES attributes, since no such a table exists in the database)*/
	
	/*char * tableName = "test420";
	AK_create_table_parameter * parameters = (AK_create_table_parameter *) AK_malloc(sizeof(AK_create_table_parameter));
	AK_create_table_parameter * parameter = (AK_create_table_parameter *) AK_malloc(sizeof(AK_create_table_parameter));
	char * parName;
	for(int i = 0; i < 13; i++){
		switch(i){
			case 0:{
				parName = "par0";
				break;}
			case 1:{
				parName = "par1";
				break;}
			case 2:{
				parName = "par2";
				break;}
			case 3:{
				parName = "par3";
				break;}
			case 4:{
				parName = "par4";
				break;}
			case 5:{
				parName = "par5";
				break;}
			case 6:{
				parName = "par6";
				break;}
			case 7:{
				parName = "par7";
				break;}
			case 8:{
				parName = "par8";
				break;}
			case 9:{
				parName = "par9";
				break;}
			case 10:{
				parName = "par10";
				break;}
			case 11:{
				parName = "par11";
				break;}
			case 12:{
				parName = "par12";
				break;}
			
			}
		parameter = AK_create_create_table_parameter(TYPE_INT, parName);
		parameters[i] = *parameter;
		}
	
	//AK_InsertAtEnd_L3(TYPE_ATTRIBS, parameter, sizeof(parameter), parameters);
	AK_create_table(tableName, parameters, 13);
	
	struct list_node *row_root = (struct list_node *) AK_malloc(sizeof (struct list_node));
    AK_Init_L3(&row_root);
    int par0 = 5;
    int par1 = 15;
    int par2 = 25;
    int par3 = 35;
    int par4 = 45;
    int par5 = 55;
    int par6 = 65;
    int par7 = 75;
    int par8 = 85;
    int par9 = 95;
    int par10 = 105;
    int par11 = 115;
    int par12 = 125;
    
    AK_Insert_New_Element(TYPE_INT, &par0, tableName, "par0", row_root);
    AK_Insert_New_Element(TYPE_INT, &par1, tableName, "par1", row_root);
    AK_Insert_New_Element(TYPE_INT, &par2, tableName, "par2", row_root);
    AK_Insert_New_Element(TYPE_INT, &par3, tableName, "par3", row_root);
    AK_Insert_New_Element(TYPE_INT, &par4, tableName, "par4", row_root);
    AK_Insert_New_Element(TYPE_INT, &par5, tableName, "par5", row_root);
    AK_Insert_New_Element(TYPE_INT, &par6, tableName, "par6", row_root);
    AK_Insert_New_Element(TYPE_INT, &par7, tableName, "par7", row_root);
    AK_Insert_New_Element(TYPE_INT, &par8, tableName, "par8", row_root);
    AK_Insert_New_Element(TYPE_INT, &par9, tableName, "par9", row_root);
    AK_Insert_New_Element(TYPE_INT, &par10, tableName, "par10", row_root);
    AK_Insert_New_Element(TYPE_INT, &par11, tableName, "par11", row_root);
    AK_Insert_New_Element(TYPE_INT, &par12, tableName, "par12", row_root);
    
    AK_insert_row(row_root);
    
    AK_DeleteAll_L3(&row_root);
    
    par0 = 8;
    par1 = 18;
    par2 = 28;
    par3 = 38;
    par4 = 48;
    par5 = 58;
    par6 = 68;
    par7 = 78;
    par8 = 88;
    par9 = 98;
    par10 = 108;
    par11 = 118;
    par12 = 128;
    
    AK_Insert_New_Element(TYPE_INT, &par0, tableName, "par0", row_root);
    AK_Insert_New_Element(TYPE_INT, &par1, tableName, "par1", row_root);
    AK_Insert_New_Element(TYPE_INT, &par2, tableName, "par2", row_root);
    AK_Insert_New_Element(TYPE_INT, &par3, tableName, "par3", row_root);
    AK_Insert_New_Element(TYPE_INT, &par4, tableName, "par4", row_root);
    AK_Insert_New_Element(TYPE_INT, &par5, tableName, "par5", row_root);
    AK_Insert_New_Element(TYPE_INT, &par6, tableName, "par6", row_root);
    AK_Insert_New_Element(TYPE_INT, &par7, tableName, "par7", row_root);
    AK_Insert_New_Element(TYPE_INT, &par8, tableName, "par8", row_root);
    AK_Insert_New_Element(TYPE_INT, &par9, tableName, "par9", row_root);
    AK_Insert_New_Element(TYPE_INT, &par10, tableName, "par10", row_root);
    AK_Insert_New_Element(TYPE_INT, &par11, tableName, "par11", row_root);
    AK_Insert_New_Element(TYPE_INT, &par12, tableName, "par12", row_root);
    
    AK_insert_row(row_root);
    AK_insert_row(row_root);
    AK_insert_row(row_root);
    AK_DeleteAll_L3(&row_root);
	
	AK_print_table(tableName);
	
	struct list_node * lista = AK_get_column(11, "test420");
	struct list_node * el = AK_First_L2(lista);
	void *data = (void *) AK_calloc(MAX_VARCHAR_LENGTH, sizeof (void));
	memcpy(data, el->data, sizeof (int));
    printf("\n%*i |\n", 5, *((int *) data));
    el = AK_Next_L2(el);
    memcpy(data, el->data, sizeof (int));
    printf("\n%*i |\n", 5, *((int *) data));*/
    
    /* ---------------------------------------------------------------------------------------------- */
	

    AK_PRO;
    printf("table.c: Present!\n");

    printf("\n********** TABLE ABSTRACTION TEST by Matija Šestak **********\n\n");

    printf("Table \"student\":AK_print_table\n");
    AK_print_table("student");
    printf("\n");

    printf("Table \"student\": AK_table_empty: ");
    if (AK_table_empty("student")) {
        printf("true\n");
    } else {
        printf("false\n");
    }
    printf("\n");

    printf("Table \"student\": AK_num_attr: ");
    printf("%d\n", AK_num_attr("student"));
    printf("\n");
	
    char get_num_records = AK_get_num_records("student");
    printf("Table \"student\": AK_get_num_records: ");
    printf("%d\n", get_num_records);
    printf("\n");



    printf("Table \"student\": AK_get_row: ");
    
    int i;

    AK_header *head = AK_get_header("student");
    int num_attr = AK_num_attr("student");
    int len[num_attr];
    for (i = 0; i < num_attr; i++) {
            len[i] = strlen((head + i)->att_name);
    }

    AK_print_row(len, AK_get_row(0,"student"));
    printf("\n");

    printf("Table \"student\": AK_get_attr_name for index 3: ");
    char *get_attr_name = AK_get_attr_name("student", 3);
    
    printf("%s\n", get_attr_name);
    printf("\n");

    
    

    int get_attr_index;
    printf("Table \"student\": AK_get_attr_index of \"year\": ");
    printf("%d\n", get_attr_index = AK_get_attr_index("student", "year"));
    printf("\n");
	
    char *tuple_to_string = AK_tuple_to_string(AK_get_tuple(0, 1, "student"));
    printf("Table \"student\": AK_get_tuple for row=0, column=1:");
    printf("%s\n", tuple_to_string);
	
    const int testConditions[] = {
        get_num_records != EXIT_WARNING, 
        get_attr_name != NULL, 
        get_attr_index != EXIT_WARNING, 
        tuple_to_string != NULL
    };
    
    unsigned short successfulTests = 0, failedTests = 0;
    for(size_t i = 0; i < sizeof testConditions / sizeof testConditions[0]; i++) {
        successfulTests += testConditions[i];
        failedTests += !testConditions[i];
    }
	
    char * table_name = "table_c_create_table_test";

    printf("\nTable \"%s\":AK_create_table\n", table_name);

    AK_create_table_parameter *params = (AK_create_table_parameter *) AK_malloc(2 * sizeof(AK_create_table_parameter*));

    params[0] = *(AK_create_create_table_parameter(TYPE_INT, "ID"));
    params[1] = *(AK_create_create_table_parameter(TYPE_VARCHAR, "Name"));

    AK_create_table(table_name, params, 2);

    if(AK_table_exist(table_name)){
        successfulTests++;
    }
    else{
        printf("Table \"%s\" doesn't exists.", table_name);
        failedTests++;
    }

    if(AK_num_attr("table_c_create_table_test") == 2){
        successfulTests++;
    }
    else{
        printf("Table \"%s\" should have 2 attributes.", table_name);
        failedTests++;
    }    
    
    AK_free(params);

    AK_EPI;
    return TEST_result(successfulTests, failedTests);
}
/**
 * @author Mislav Čakarić, edited by Ljubo Barać
 * @brief Function for renaming operator testing (moved from rename.c)
 * @return TestResult containing information on the amount of failed/passed tests
 */
TestResult AK_op_rename_test() {
    AK_PRO;
    //printf( "rename_test: Present!\n" );
    printf("\n********** RENAME TEST-rename table assistant **********\n\n");

    AK_print_table("AK_relation");
    AK_rename("assistant", ".", "assistantNew", ".");
    AK_print_table("AK_relation");
    AK_rename("assistantNew", ".", "assistant", ".");
    AK_print_table("AK_relation");


    printf("\n********** RENAME TEST-rename attribute weight (table student) **********\n\n");
    AK_print_table("student");
    AK_rename("student", "weight", "student", "weightNew");
    AK_print_table("student");
    AK_rename("student", "weightNew", "student", "weight");
    AK_print_table("student");

    printf("\n\n------------END OF RENAME TEST------------\n\n");

	
   AK_EPI; 
	if (rename != EXIT_ERROR ){
	  return TEST_result(1,0);
    }
    else{
	  return TEST_result(0,1);
    }

}
