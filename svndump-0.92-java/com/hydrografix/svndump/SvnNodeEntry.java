/*
 * Licensed under the BSD License
 * (http://www.opensource.org/licenses/bsd-license.php)
 * by Stefan Hegny, hydrografix Consulting GmbH,
 * Frankfurt/Main, Germany
 * and others, see http://svn2cc.sarovar.org
 * November 2005-
 */


package com.hydrografix.svndump;

import java.io.IOException;
import java.io.InputStream;



/**
 * class describing a modified file (node) within a revision
 * @author hegny
 *
 */
public class SvnNodeEntry
{
	/**
	 * 
	 */
	private final SvnDump svnDump;
	/**
	 * if this is true, it's not considered because on another branch
	 */
	public boolean ignored = true;
	/**
	 * name of the file, chopped by the "main/" trunk repository part
	 */
	public final String			name;
	
	/**
	 * name including svn repository path
	 */
	public final String			fullName;
	
	/**
	 * type of node
	 * @see SvnNodeEntry#NODEKIND_DIR
	 * @see SvnNodeEntry#NODEKIND_FILE
	 * @see SvnNodeEntry#NODEKIND_UNKNOWN
	 */
	public int				type = SvnNodeEntry.NODEKIND_UNKNOWN;
	/**
	 * action on node
	 * @see SvnNodeEntry#NODEACT_ADD
	 * @see SvnNodeEntry#NODEACT_DELETE
	 * @see SvnNodeEntry#NODEACT_CHANGE
	 * @see SvnNodeEntry#NODEACT_UNKNOWN
	 * @see SvnNodeEntry#NODEACT_COPY_OR_MOVE
	 */
	public int 				action;
	/**
	 * the contents if given
	 */
	public byte[]			contents = null;
	/**
	 * length of properties given for node
	 */
	public int				propLength = 0;
	/** 
	 * length of node textdata
	 */
	public int				textLength = 0;
	/**
	 * source of copy of node (stripped of branch path)
	 */
	public String			src = null;
	/**
	 * source of copy of node (incl. branch path)
	 */
	public String			fullSrcPath = null;
	/**
	 * node was moved to somwhere else
	 * (this is not contained in the dump)
	 * (only used in the FileChangeSet)
	 */
	public static final int		NODEACT_REMOVE		= 5;
	/**
	 * node was moved from somwhere else
	 * (this is not contained in the dump)
	 * (only used in the FileChangeSet)
	 */
	public static final int		NODEACT_MOVE		= 4;
	/**
	 * not clear if moved (if deleted afterwards) or
	 * added as copy (which can not be modeled straight in ccase).
	 * Will be used on create of SvnNodeEntry iff
	 * action is add and source is given.
	 * Will be modified after importing all files
	 * of a revision to NODEACT_ADD (copy which can
	 * not be modeled in ccase) or NODEACT_MOVE
	 */
	public static final int		NODEACT_COPY_OR_MOVE = 3;
	/**
	 * node was deleted
	 */
	public static final int		NODEACT_DELETE		= 2;
	/**
	 * Node was added or copied from other location
	 */
	public static final int		NODEACT_ADD			= 1;
	/**
	 * node was modified
	 */
	public static final int 	NODEACT_CHANGE		= 0;
	/**
	 * unknown action
	 */
	public static final int 	NODEACT_UNKNOWN		= -1;
	/**
	 * Node is a directory
	 */
	public static final int		NODEKIND_DIR		= 1;
	/**
	 * Node is a file
	 */
	public static final int		NODEKIND_FILE		= 0;
	/**
	 * unknown type of node
	 */
	public static final int 	NODEKIND_UNKNOWN	= -1;
	
	/**
	 * create file reading from input stream
	 * @param s input stream
	 * @param fname file name
	 * @param dump containing dump instance
	 */
	public SvnNodeEntry(SvnDump dump, InputStream s, String fname, SvnRevision svnrev)
	{
		svnDump = dump;
		fullName = fname;
		if (fname.startsWith(svnDump.filterTrunk))
		{
			name = fname.substring(svnDump.filterTrunk.length());
			if (0 < name.length())
				ignored = false;
		}
		else
			name = null;

		String t = dump.readLine(s);
		String val;
		do
		{
			if (t.startsWith("Node-kind:"))
			{
				val = t.substring(11);
				if (val.equalsIgnoreCase("dir"))
					type = SvnNodeEntry.NODEKIND_DIR;
				else if (val.equalsIgnoreCase("file"))
					type = SvnNodeEntry.NODEKIND_FILE;
				else
					type = SvnNodeEntry.NODEKIND_UNKNOWN;
			}
			else if (t.startsWith("Node-action"))
			{
				val = t.substring(13);
				if (val.equalsIgnoreCase("delete"))
					action = SvnNodeEntry.NODEACT_DELETE;
				else if (val.equalsIgnoreCase("add"))
				{
					if (!ignored && (0 < svnDump.dirname(name).length()))
						action = SvnNodeEntry.NODEACT_ADD;
					else
						ignored = true;
				}
				else if (val.equalsIgnoreCase("change"))
					action = SvnNodeEntry.NODEACT_CHANGE;
				else
					action = SvnNodeEntry.NODEACT_UNKNOWN;
			}
			else if (t.startsWith("Node-copyfrom-path"))
			{
				src = t.substring(20);
				if (src.startsWith(svnDump.filterTrunk))
				{
					fullSrcPath = src;
					src = src.substring(svnDump.filterTrunk.length());
				}
				else
					src = null;
			}
			else if (t.startsWith("Text-content-length:"))
			{
				val = t.substring(21);
				textLength = Integer.parseInt(val);
			}
			else if (t.startsWith("Prop-content-length:"))
			{
				val = t.substring(21);
				propLength = Integer.parseInt(val);
			}
			t = dump.readLine(s);
		} while ((t.length() > 0) && !dump.eofInput);

		// check if it's real add or possibly copy_or_move
		if ((null != src) && (action == NODEACT_ADD))
		{
			// we don't really know at the moment
			action = NODEACT_COPY_OR_MOVE;
		}
		
		if (0 < propLength)
		{
			try {
				s.skip(propLength);
			}
			catch (IOException ioe) { ioe.printStackTrace(); }
		}
		if (0 < textLength)
		{
			contents = new byte[textLength];
			try {
				s.read(contents, 0, textLength);
			}
			catch (IOException ioe) { ioe.printStackTrace(); }
		}
		String nxt = dump.readLine(s);
		if (0 <= nxt.length())
			dump.pushBackInputLine(nxt);
		
	}
	
	/**
	 * string representation
	 */
	public String toString()
	{
		return ("SvnNodeEntry [" + name + "]");
	}

	/**
	 * return string representation of element type
	 * @see #NODEKIND_DIR
	 * @see #NODEKIND_FILE
	 * @see #NODEKIND_UNKNOWN
	 * @param nodetype
	 * @return string representation
	 */
	public static String typeToString(int nodetype)
	{
		switch (nodetype)
		{
		case NODEKIND_DIR:
			return "dir";
		case NODEKIND_FILE:
			return "file";
		default:
			return "element";
		}
	}
}