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
import java.text.ParseException;
import java.util.Enumeration;
import java.util.Vector;



/**
 * class describes a dumped revision.
 * Note: creating the revision will import it from
 * the input stream given.
 * To do anything else to the revision, subclass
 * this class
 * @author hegny
 * @see com.hydrografix.svndump.SvnNodeEntry
 * @see com.hydrografix.svndump.toccase.RevToCCase
 */
public class SvnRevision
{
	/**
	 * 
	 */
	private final SvnDump svnDump;
	/** 
	 * false if any files found
	 */
	boolean isempty = true;
	/**
	 * log comment
	 * also to be used to build separate zip containing only the 
	 * batches and comments
	 */
	public String			descr = "";
	/**
	 * author/creator
	 */
	public String			author = "";
	/**
	 * date in the string format from the dump
	 */
	public String			date = "";
	/**
	 * the date as long
	 */
	public long				datetime;
	/**
	 * revision number
	 */
	public int				num;
	/**
	 * contains SvnNodeEntry elements for file data in this revision
	 */
	public Vector			files = new Vector();
	
	/**
	 * text of all checkout commands.
	 * To be used to build separate zip containing only the 
	 * batches and comments
	 */
	public String 			checkoutLines = "";
	/**
	 * text of all checkin commands.
	 * To be used to build separate zip containing only the 
	 * batches and comments
	 */
	public String			checkinLines = "";
	
	/**
	 * create revision reading from input stream
	 * @param s the input stream (dump file)
	 * @param number revision number
	 * @param dump containing dump instance
	 */
	public SvnRevision(SvnDump dump, InputStream s, int number)
	{
		svnDump = dump;
		num = number;
		/* skip rest of revision definition */
		while (svnDump.readLine(s).length() > 0);
		/* key-value pairs containing log, date etc. */
		String t = svnDump.readLine(s);
		int len;
		String key = "";
		String val = "";
		do
		{
			if (t.startsWith("K "))
			{
				len = Integer.parseInt(t.substring(2));
				byte kb[] = new byte[len];
				try {
					s.read(kb, 0, len);
					key = new String(kb);
				} catch (IOException ioe) { ioe.printStackTrace(); }
				svnDump.readLine(s);
				t = svnDump.readLine(s);
			}
			else if (t.startsWith("V "))
			{
				len = Integer.parseInt(t.substring(2));
				byte vb[] = new byte[len];
				try {
					s.read(vb, 0, len);
					val = new String(vb, "UTF8");
				} catch (IOException ioe) { ioe.printStackTrace(); }
				if (key.toLowerCase().endsWith(":log"))
				{
					descr = val;
				}
				else if (key.toLowerCase().endsWith(":author"))
				{
					author = val;
				}
				else if (key.toLowerCase().endsWith(":date"))
				{
					date = val;
					try {
						datetime = SvnDump.dtf.parse(date.substring(0,19)).getTime() + SvnDump.tzOffset;
					}
					catch (ParseException pe) { pe.printStackTrace(); }
				}
				key = "";
				svnDump.readLine(s);
				t = svnDump.readLine(s);
			}
		} while (t.length() > 0 && !t.equalsIgnoreCase("PROPS-END"));
		
		if (-1 != descr.indexOf(13))
			descr += (SvnDump.DOS_CRLF);
		else
			descr += ("" + (char)10);
		descr += ("[svn:"+num+":"+author+","+date+"]");
		
		do 
		{
			t = svnDump.readLine(s);
		} while ((!svnDump.eofInput) && (0 >= t.length()));
		
		while (!t.startsWith("Revision-number:") && !svnDump.eofInput)
		{
			if (t.startsWith("Node-path:"))
			{
				SvnNodeEntry fi = dump.createFile(this, s, t.substring(11));
				boolean includeElem = false;
				if (!fi.ignored)
					includeElem = svnDump.checkForInclusion(fi.name, num);
				if (!fi.ignored && includeElem)
				{
					files.add(fi);
					isempty = false;
					if (2 <= svnDump.verbosity)
						System.out.println("Imported " + fi.toString());
				}
				else if (2 <= svnDump.verbosity)
					System.out.println("Excluded non-relevant \"" + fi.fullName + "\"");
			}
			do 
			{
				t = svnDump.readLine(s);
			} while ((!svnDump.eofInput) && (0 >= t.length()));
		}
		if (0 < t.length())
			svnDump.pushBackInputLine(t);
		
		/* fix files with unclear copy_or_move action
		 * depending on a delete for the file occuring
		 * later in the revision import (then it's move)
		 * or not (then it's add)
		 */
		for (int i=0;i<files.size();i++)
		{
			SvnNodeEntry ne = (SvnNodeEntry)files.elementAt(i);
			if (SvnNodeEntry.NODEACT_COPY_OR_MOVE == ne.action)
			{
				boolean deleted = false;
				int delindex = 0;
				for (int j=i+1;j<files.size();j++)
				{
					SvnNodeEntry ne2 = (SvnNodeEntry)files.elementAt(j);
					if ((SvnNodeEntry.NODEACT_DELETE == ne2.action) &&
							(ne2.fullName.equals(ne.fullSrcPath)))
					{
						deleted = true;
						delindex = j;
						break;
					}
				}
				if (deleted)
				{
					ne.action = SvnNodeEntry.NODEACT_MOVE;
					/* also remove the DELETE file node */
					files.removeElementAt(delindex);
				}
				else
					ne.action = SvnNodeEntry.NODEACT_ADD;
			}
		}
	}
	
	/**
	 * string representation
	 */
	public String toString()
	{
		return ("SvnRevision [" + num + ", " + files.size() + " file(s)]");
	}
	
	/**
	 * check if file contents for a given filename is
	 * contained in the revision.
	 * @param filename filename to check
	 * @return true if contents is here
	 */
	public boolean containsChangedDataFor(String filename)
	{
		boolean rc = false;
		Enumeration e = files.elements();
		while (e.hasMoreElements())
		{
			SvnNodeEntry fe = (SvnNodeEntry)e.nextElement();
			if (fe.name.equals(filename) && 
					((fe.action == SvnNodeEntry.NODEACT_ADD) ||
							(fe.action == SvnNodeEntry.NODEACT_CHANGE) ||
							(fe.action == SvnNodeEntry.NODEACT_MOVE)))
			{
				if (!fe.ignored && fe.contents != null)
					rc = true;
				break;
			}
		}
		return rc;
	}
}