/*
 * Licensed under the BSD License
 * (http://www.opensource.org/licenses/bsd-license.php)
 * by Stefan Hegny, hydrografix Consulting GmbH,
 * Frankfurt/Main, Germany
 * and others, see http://svn2cc.sarovar.org
 * November 2005-
 */


package com.hydrografix.svndump;

import java.io.*;
import java.util.*;
import java.text.*;


/**
 * Parse and rearrange a svnadmin dump.
 * Create the dump with <br>
 * <code>svnadmin dump --incremental -r&lt;startrev>:&lt;endrev> &lt;repository> &gt;outfile</code>
 * <br>
 * The creator will import the dump.
 * To do anything further, subclass me.
 * @author hegny
 * @see com.hydrografix.svndump.SvnRevision
 * @see com.hydrografix.svndump.toccase.SvnToCCase
 *
 */
public class SvnDump {

	InputStream			input;
	
	/**
	 * the name of the trunk in my repository.
	 * This one is filtered out and used on import;
	 * others are discarded.
	 */
	public final String 			filterTrunk;
	
	/**
	 * for conversion date -> datetime
	 */
	protected static SimpleDateFormat	dtf = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss");
	
	/**
	 * timezone offset, the svn times are Zulu
	 */
	protected static int tzOffset = new GregorianCalendar().get(GregorianCalendar.ZONE_OFFSET) + 
							new GregorianCalendar().get(GregorianCalendar.DST_OFFSET);
	
	/**
	 * reference list which node was modified in which revision
	 */
	protected Vector		fileRef = new Vector();
	
	/**
	 * change sets
	 */
	protected Vector		chgSets = new Vector();
	
	/**
	 * constant for batch strings
	 */
	public static final String	DOS_CRLF = "" + (char)13 + (char)10;
	
	/**
	 * 0: quiet; 1: normal; 2: fully verbose
	 */
	public final int 		verbosity;
	
	/**
	 * contains SvnRevision elements from input
	 */
	public Vector revisions = new Vector();
	
	/**
	 * contains files to be excluded 
	 * of type ExIncludeFilterElement
	 */
	Vector	excludedElements = null;
	
	/**
	 * contains files to be included 
	 * of type ExIncludeFilterElement
	 */
	Vector	includedElements = null;
	
	/**
	 * flag if input file on EOF
	 */
	public boolean 	eofInput		= false;
	
	/**
	 * so a line can be pushed-back after read
	 */
	private String	lastLine		= null;
	
	/**
	 * build revisions from input dump using default filter "trunk"
	 * @param filename input file name
	 */
	public SvnDump(String filename)
	{
		this(filename, "trunk/", 1, null, null);
	}
	
	/**
	 * create dump object by importing dump file
	 * @param filename the svn dump file
	 * @param filter name of svn repository directory to filter out (e.g. "trunk" or "branch/something")
	 * @param verbose 0 means quiet, 1 normal and 2 more verbose output
	 * @param excluded vector of strings of element names to be ignored
	 * @param included vector of strings of element names to be included anyway
	 * or if no excluded given, then the only things to be included (all other excluded)
	 */
	public SvnDump(String filename, String filter, int verbose, Vector excluded, Vector included)
	{
		if (null == filter)
			filter = "trunk/";
		if (!filter.endsWith("/"))
			filterTrunk = filter + "/";
		else
			filterTrunk = filter;
		
		verbosity = verbose;
		excludedElements = excluded;
		if ((null != excludedElements) && (0 == excludedElements.size()))
			excludedElements = null;
		includedElements = included;
		if ((null != includedElements) && (0 == includedElements.size()))
			includedElements = null;
		
		if (null != filename)
		{
			try {
				input = new FileInputStream (filename);
			} catch (FileNotFoundException fe)
			{
				System.out.println("Error opening input File.");
				System.out.println(fe.toString());
				return;
			}
		}
		else
			input = System.in;
		
		String t = readLine(input);
		while (!t.startsWith("Revision-number:"))
			t = readLine(input);
		
		SvnRevision r;
		do
		{
			r = createRevision(input, Integer.parseInt(t.substring(17)));
			if (!r.isempty)
			{
				revisions.add(r);
				if (1 <= verbosity)
					System.out.println("Imported " + r.toString());
			}
			else if (1 <= verbosity)
				System.out.println("Excluded empty " + r.toString());
			t = readLine(input);
		} while ((0 < t.length()) && !eofInput);
	}
	
	/**
	 * convert string to byte array and write to (zip)stream
	 * @param os the output stream
	 * @param s string to be written
	 * @throws IOException
	 */
	public static void writeStringToStream(OutputStream os, String s) throws IOException
	{
		byte commstr[] = s.getBytes();
		os.write(commstr);
	}
	
	/**
	 * read string up to newline from input stream
	 * @param sin the input stream
	 * @return all characters except the newline
	 */
	public String readLine(InputStream sin)
	{
		if (null != lastLine)
		{
			String tmp = lastLine;
			lastLine = null;
			return tmp;
		}
		
		byte bytes[] = new byte[9999];
		int b = 0;
		int cnt = 0;
		
		while (b != 10)
		{
			try {
				b = sin.read();
				if (-1 == b)
				{
					eofInput = true;
					break;
				}
				bytes[cnt++] = (byte)b;
			}
			catch (IOException ioe)
			{
				eofInput = true;
				break;
			}
		}
		if (0 < cnt)
		{
			String erg;
			try {
				erg = new String(bytes, 0, cnt-1, "UTF8");
			} 
			catch (UnsupportedEncodingException uee) 
			{ 
				uee.printStackTrace(); 
				erg = new String(bytes, 0, cnt - 1);
			}
			return erg;
		}
		else 
			return "";
	}
	
	public void pushBackInputLine(String input)
	{
		lastLine = input;
	}
	
	/**
	 * check for position of an item of the form "str';'" in the vector
	 * @param str the string to be searched
	 * @param v the vector to be searched
	 * @return the position of the item, or -1 if not found
	 */
	public static int headPosition(String str, Vector v)
	{
		int result = 0;
		Enumeration e = v.elements();
		while (e.hasMoreElements())
		{
			String s = (String)e.nextElement();
			if (s.startsWith(str + ";"))
				return (result);
			result++;
		}
		return -1;
	}
	
	/**
	 * insert str into vector sorted alphabetically
	 * @param str the str (element name, in this case)
	 * @param v the vector
	 * @return index of inserted vector item 
	 */
	public static int sortedHeadInsert(String str, Vector v)
	{
		int index = 0;
		Enumeration e = v.elements();
		while (e.hasMoreElements())
		{
			String st = (String)e.nextElement();
			if (-1 < st.indexOf(";"))
				st = st.substring(0, st.indexOf(";"));
			if (0 < st.compareTo(str))
			{
				if (!str.endsWith(";"))
					str += ";";
				v.add(index, str);
				return index;
			}
			else
				index++;
		}
		if (!str.endsWith(";"))
			str += ";";
		v.add(str);
		return (v.size() - 1);
	}
	
	/**
	 * primitive dir name extraction
	 * @param path path and file name
	 * @return path part
	 */
	public String dirname(String path)
	{
		int l = path.lastIndexOf('/');
		if (-1 == l)
			return "";
		else
			return path.substring(0, l);
	}

	/**
	 * check for occurence of element name/revision number
	 * in either exclude or include list.
	 * @param filename element name
	 * @param revno revision number
	 * @param exin vector or ExIncludeFilterElement
	 * @return true if matched in the list
	 */
	protected boolean isInExIncludeList(String filename, int revno, Vector exin)
	{
		if (null == exin)
			return false;
		
		Enumeration en = exin.elements();
		ExIncludeFilterElement ele;
		while (en.hasMoreElements())
		{
			ele = (ExIncludeFilterElement)en.nextElement();
			if (((!ele.isPrefixWildcard) && (ele.nameOrPrefix.equals(filename))) ||
					(ele.isPrefixWildcard && (filename.startsWith(ele.nameOrPrefix))))
			{
				return ((revno < ele.onlyBelowRevision) && (revno > ele.onlyAboveRevision));
			}
		}
		return false;
	}
	
	/**
	 * check if a given element is included using
	 * the included- and excluded-lists
	 * @param filename element name
	 * @param revno revision number
	 * @return true for included
	 */
	boolean checkForInclusion(String filename, int revno)
	{
		if (isInExIncludeList(filename, revno, includedElements))
			return true;
		else if (isInExIncludeList(filename, revno, excludedElements))
			return false;
		else if ((null == excludedElements) && (null != includedElements))
			return false;
		return true;
	}
	
	/**
	 * Make object from command line parameter.
	 * From a command line parameter of the form
	 * <code>element-name-or-prefix[*][&gt;greater-rev|&lt;below-rev]</code>
	 * a corresponding object for exclude- or include-filtering.
	 * @param input argument string
	 * @return filter object
	 */
	public static ExIncludeFilterElement parseExInclude(String input)
	{
		int asteriskIndex = input.lastIndexOf("*");
		int nameLength = 0;
		boolean wildcard = false;
		int greaterRev = 0;
		int lessRev = 0;
		if (-1 != asteriskIndex)
		{
			nameLength = asteriskIndex;
			wildcard = true;
		}
		
		int greaterIndex = input.lastIndexOf(">");
		if (-1 != greaterIndex)
		{
			if (0 == nameLength)
				nameLength = greaterIndex;
			greaterRev = Integer.parseInt(input.substring(greaterIndex + 1));
		}
		else
		{
			int lessIndex = input.lastIndexOf("<");
			if (-1 != lessIndex)
			{
				if (0 == nameLength)
					nameLength = lessIndex;
				lessRev = Integer.parseInt(input.substring(lessIndex + 1));
			}
		}
		if (0 == nameLength)
			nameLength = input.length();
		return new ExIncludeFilterElement(input.substring(0, nameLength), wildcard, lessRev, greaterRev);
	}
	
	/**
	 * overwrite this function if you have subclassed SvnRevision
	 * @param s stream to read input from
	 * @param number revision number (has already been read)
	 * @return a SvnRevision or subclass
	 */
	public SvnRevision createRevision(InputStream s, int number)
	{
		return new SvnRevision(this, s, number);
	}
	
	/**
	 * overwrite this function if you have subclassed SvnNodeEntry
	 * @param rev corresponding SvnRevision or subclass
	 * @param s input stream of dump
	 * @param name Node name
	 * @return new SvnNodeEntry or subclass
	 */
	public SvnNodeEntry createFile(SvnRevision rev, InputStream s, String name)
	{
		return new SvnNodeEntry(this, s, name, rev);
	}

}
