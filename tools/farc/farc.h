#ifndef FILEARCHIVE_FARC_H
#define FILEARCHIVE_FARC_H

/*!
 * \defgroup farc
 * \brief File Archive Creator
 * \{
 */

/*!
 * \page farc.description Description
 * 
 * This tool is used to allow for easy creation and access to file archives, either created by the same tool or by the API. It will allow creating both raw and compressed archives, enumerate the contents within, and also output data from entries within the archive.
 */

/*!
 * \page farc.usage Usage
 *
 * The main executable is command-driven, giving each and every command control over the remaining attributes. The main format for the arguments are as follows:
 * \verbatim farc <command> <options> <archive> ... \endverbatim
 * Anything not related to the command result is written to stderr, allowing you to pipe the result without including error reports.
 */

/*!
 * \page farc.commands Commands
 *
 * \verbatim create <options> <archive> <file> ... [@<spec> ...] \endverbatim
 * 
 * Creates a new archive. Options are as follows:
 * \li <tt>-z <em>\<method\></em></tt>		Compression method used for the archive; available methods are \b none and \b fastlz
 * \li <tt>-s</tt>			Pad files and structures to align with 2048 sector size (appropriate for DVD media)
 * \li <tt>-v</tt>			Enable verbose command output, displaying information about every file added to the archive
 *
 * \verbatim list <archive> ... \endverbatim
 *
 * List content within on or more file archives.
 *
 * \verbatim cat <archive> <file> ... \endverbatim
 *
 * Output the content of one or more entries within a file archive.
*/
/*! \cond HIDDEN */

int commandCreate(int argc, char* argv[]);
int commandList(int argc, char* argv[]);
int commandCat(int argc, char* argv[]);
int commandHelp(const char* command);

/*! \endcond */

/*!
 * \}
 */

#endif

