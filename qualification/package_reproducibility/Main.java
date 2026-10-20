// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package reproducibility;

import generated.Generated.SerializerPackage;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HexFormat;
import java.util.List;

/** Produce package bytes through public generated APIs without adapter-side canonical sorting. */
public final class Main {
  /** Quote runtime metadata independently of the generated model and package serializers. */
  private static String quote(String value) {
    var text=new StringBuilder("\"");
    for(int index=0;index<value.length();++index) {
      char item=value.charAt(index);
      if(item=='"'||item=='\\')text.append('\\').append(item);
      else if(item<32)text.append(String.format(java.util.Locale.ROOT,"\\u%04x",(int)item));
      else text.append(item);
    }
    return text.append('"').toString();
  }

  /** Identify the actual JDK provider and the package writer's existing raw-DEFLATE options. */
  private static String provider(int metadataMethod) {
    return "{\"implementation\":\"java.util.zip.Deflater\",\"version\":"+quote(Runtime.version().toString())+
      ",\"options\":{\"format\":\"raw_deflate\",\"level\":6,\"window_bits\":15,\"nowrap\":true,\"strategy\":0,\"dictionary\":\"none\",\"flush\":\"finish\"}"+
      (metadataMethod<0?"":",\"metadata_method\":"+metadataMethod)+"}";
  }

  /** Apply one of the six fixed list permutations while preserving every authored field. */
  private static <T> List<T> permute(List<T> values,int permutation) {
    var result=new ArrayList<T>();int count=values.size();
    if(permutation<0||permutation>5)throw new IllegalArgumentException("Expected permutation 0 through 5");
    if(count==0)return result;
    if(permutation==0)result.addAll(values);
    else if(permutation==1)for(int index=count-1;index>=0;--index)result.add(values.get(index));
    else if(permutation==2||permutation==3){int offset=permutation==2?1:count/2;for(int index=0;index<count;++index)result.add(values.get((index+offset)%count));}
    else if(permutation==4){for(int parity=0;parity<2;++parity)for(int index=parity;index<count;index+=2)result.add(values.get(index));}
    else{for(int parity=1;parity>=0;--parity)for(int index=count-1;index>=0;--index)if(index%2==parity)result.add(values.get(index));}
    return result;
  }

  /** Decode the no-header UTF-8 fixture without trimming or normalizing authored text. */
  private static List<String[]> rows(Path path,int columns) throws Exception {
    var result=new ArrayList<String[]>();
    for(String line:Files.readAllLines(path,StandardCharsets.UTF_8)) {
      if(line.isEmpty())continue;
      String[] values=line.split("\t",-1);if(values.length!=columns)throw new IllegalArgumentException("Invalid fixture column count");result.add(values);
    }
    return result;
  }

  /** Produce one fresh archive, leaving metadata/compression/order decisions to generated writers. */
  public static void main(String[] args) throws Exception {
    if(args.length==1&&args[0].equals("--provider")){System.out.println(provider(-1));return;}
    if(args.length<4||args.length>5)throw new IllegalArgumentException("Expected FIXTURES PROFILE PERMUTATION OUTPUT [FORCE_ZIP64]");
    String profile=args[1];if(!List.of("zip-stored","zip-deflate","opc-stored","opc-deflate").contains(profile))throw new IllegalArgumentException("Unknown profile");
    boolean opc=profile.startsWith("opc-"),force=args.length==5&&args[4].equals("1");if(args.length==5&&!args[4].equals("0")&&!args[4].equals("1"))throw new IllegalArgumentException("Expected force flag 0 or 1");
    int permutation=Integer.parseInt(args[2]);String compression=profile.endsWith("deflate")?"deflate":"stored";Path directory=Path.of(args[0],opc?"opc":"zip");
    var parts=new ArrayList<SerializerPackage.BinaryPart>();
    for(String[] row:rows(directory.resolve("parts.tsv"),4))parts.add(new SerializerPackage.BinaryPart(HexFormat.of().parseHex(row[3]),row[0],row[1],0,compression));
    var links=new ArrayList<SerializerPackage.Relationship>();
    for(String[] row:rows(directory.resolve("links.tsv"),5))links.add(new SerializerPackage.Relationship(row[0],row[1],row[2],row[3],row[4]));
    var limits=new SerializerPackage.Limits();limits.forceZip64=force;var budget=new SerializerPackage.Budget(limits);byte[] output;
    if(opc)output=new SerializerPackage.Composer(budget).finish(new SerializerPackage.Extensions(permute(parts,permutation),permute(links,permutation)));
    else try(var writer=new SerializerPackage.ZipWriter(budget)){for(var part:permute(parts,permutation))writer.add(part.name,part.data,part.compression);output=writer.finish();}
    Files.write(Path.of(args[3]),output);
    System.out.println("{\"profile\":"+quote(profile)+",\"permutation\":"+permutation+",\"force_zip64\":"+force+",\"parts\":"+parts.size()+",\"relationships\":"+(opc?links.size():0)+",\"provider\":"+provider(opc?8:0)+"}");
  }
}
