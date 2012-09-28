/**
 * @file   napster.h
 * @Author Anjan Karanam 
 * @date   September, 2012
 * @brief  Declaration for Napster interface. 
 *
 * Contains functions that are executed as part of Napster's client interface.
 */

#ifndef NAPSTER_H_
#define NAPSTER_H_

/**
 * Interface to add file to the central server file list.
 * Returns: 1 if file was successfully indexed, or 0 if it wasn't.
 */
int addFile(char* filename, char* client);

/**
 * Interface to delete a file from the central server file list.
 * Returns: 1 if file was successfully deleted, or 0 if it wasn't found.
 */
int deleteFile(char* filename, char* client);

/**
 * Interface to display all files that are present for downloading on the Napster server.
 */
void viewFiles();

#endif
