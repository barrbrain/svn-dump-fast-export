/*
 * Licensed under the BSD License
 * (http://www.opensource.org/licenses/bsd-license.php)
 * by Stefan Hegny, hydrografix Consulting GmbH,
 * Frankfurt/Main, Germany
 * and others, see http://svn2cc.sarovar.org
 * November 2005-
 */

package com.hydrografix.svndump;

public class ExIncludeFilterElement {
	String			nameOrPrefix;
	boolean			isPrefixWildcard;
	int				onlyBelowRevision;
	int				onlyAboveRevision;
	
	public ExIncludeFilterElement(String name, boolean wildcard, int belowRev, int aboveRev) {
		nameOrPrefix = name;
		isPrefixWildcard = wildcard;
		if (0 < belowRev)
			onlyBelowRevision = belowRev;
		else {
			onlyBelowRevision = Integer.MAX_VALUE;
			if (0 < aboveRev)
				onlyAboveRevision = aboveRev;
			else
				onlyAboveRevision = Integer.MIN_VALUE;
		}
	}

	public boolean isPrefixWildcard() {
		return isPrefixWildcard;
	}

	public String getNameOrPrefix() {
		return nameOrPrefix;
	}

	public int getOnlyAboveRevision() {
		return onlyAboveRevision;
	}

	public int getOnlyBelowRevision() {
		return onlyBelowRevision;
	}
}
