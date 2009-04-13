/**
 * Autogenerated by Thrift
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 */
package org.thrudb.thrudex;


import java.util.Set;
import java.util.HashSet;
import java.util.Collections;
import org.apache.thrift.IntRangeSet;
import java.util.Map;
import java.util.HashMap;

public class FieldType {
  public static final int KEYWORD = 1;
  public static final int TEXT = 2;
  public static final int UNSTORED = 3;

  public static final IntRangeSet VALID_VALUES = new IntRangeSet(KEYWORD, TEXT, UNSTORED);
  public static final Map<Integer, String> VALUES_TO_NAMES = new HashMap<Integer, String>() {{
    put(KEYWORD, "KEYWORD");
    put(TEXT, "TEXT");
    put(UNSTORED, "UNSTORED");
  }};
}
