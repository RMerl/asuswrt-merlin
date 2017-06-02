/*
 * Copyright (C)2009-2014 D. R. Commander.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the libjpeg-turbo Project nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS",
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

import java.io.*;
import java.awt.image.*;
import javax.imageio.*;
import java.util.*;
import org.libjpegturbo.turbojpeg.*;

class TJBench {

  static int flags = 0, quiet = 0, pf = TJ.PF_BGR, yuvpad = 1, warmup = 1;
  static boolean compOnly, decompOnly, doTile, doYUV;

  static final String[] pixFormatStr = {
    "RGB", "BGR", "RGBX", "BGRX", "XBGR", "XRGB", "GRAY"
  };

  static final String[] subNameLong = {
    "4:4:4", "4:2:2", "4:2:0", "GRAY", "4:4:0", "4:1:1"
  };

  static final String[] subName = {
    "444", "422", "420", "GRAY", "440", "411"
  };

  static final String[] csName = {
    "RGB", "YCbCr", "GRAY", "CMYK", "YCCK"
  };

  static TJScalingFactor sf;
  static int xformOp = TJTransform.OP_NONE, xformOpt = 0;
  static double benchTime = 5.0;


  static final double getTime() {
    return (double)System.nanoTime() / 1.0e9;
  }


  static String formatName(int subsamp, int cs) {
    if (cs == TJ.CS_YCbCr)
      return subNameLong[subsamp];
    else if (cs == TJ.CS_YCCK)
      return csName[cs] + " " + subNameLong[subsamp];
    else
      return csName[cs];
  }


  static String sigFig(double val, int figs) {
    String format;
    int digitsAfterDecimal = figs - (int)Math.ceil(Math.log10(Math.abs(val)));
    if (digitsAfterDecimal < 1)
      format = new String("%.0f");
    else
      format = new String("%." + digitsAfterDecimal + "f");
    return String.format(format, val);
  }


  static byte[] loadImage(String fileName, int[] w, int[] h, int pixelFormat)
                          throws Exception {
    BufferedImage img = ImageIO.read(new File(fileName));
    if (img == null)
      throw new Exception("Could not read " + fileName);
    w[0] = img.getWidth();
    h[0] = img.getHeight();
    int[] rgb = img.getRGB(0, 0, w[0], h[0], null, 0, w[0]);
    int ps = TJ.getPixelSize(pixelFormat);
    int rindex = TJ.getRedOffset(pixelFormat);
    int gindex = TJ.getGreenOffset(pixelFormat);
    int bindex = TJ.getBlueOffset(pixelFormat);
    byte[] dstBuf = new byte[w[0] * h[0] * ps];
    int pixels = w[0] * h[0], dstPtr = 0, rgbPtr = 0;
    while (pixels-- > 0) {
      dstBuf[dstPtr + rindex] = (byte)((rgb[rgbPtr] >> 16) & 0xff);
      dstBuf[dstPtr + gindex] = (byte)((rgb[rgbPtr] >> 8) & 0xff);
      dstBuf[dstPtr + bindex] = (byte)(rgb[rgbPtr] & 0xff);
      dstPtr += ps;
      rgbPtr++;
    }
    return dstBuf;
  }


  static void saveImage(String fileName, byte[] srcBuf, int w, int h,
                        int pixelFormat) throws Exception {
    BufferedImage img = new BufferedImage(w, h, BufferedImage.TYPE_INT_RGB);
    int pixels = w * h, srcPtr = 0;
    int ps = TJ.getPixelSize(pixelFormat);
    int rindex = TJ.getRedOffset(pixelFormat);
    int gindex = TJ.getGreenOffset(pixelFormat);
    int bindex = TJ.getBlueOffset(pixelFormat);
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++, srcPtr += ps) {
        int pixel = (srcBuf[srcPtr + rindex] & 0xff) << 16 |
                    (srcBuf[srcPtr + gindex] & 0xff) << 8 |
                    (srcBuf[srcPtr + bindex] & 0xff);
        img.setRGB(x, y, pixel);
      }
    }
    ImageIO.write(img, "bmp", new File(fileName));
  }


  /* Decompression test */
  static void decomp(byte[] srcBuf, byte[][] jpegBuf, int[] jpegSize,
                     byte[] dstBuf, int w, int h, int subsamp, int jpegQual,
                     String fileName, int tilew, int tileh) throws Exception {
    String qualStr = new String(""), sizeStr, tempStr;
    TJDecompressor tjd;
    double elapsed, elapsedDecode;
    int ps = TJ.getPixelSize(pf), i, iter = 0;
    int scaledw = sf.getScaled(w);
    int scaledh = sf.getScaled(h);
    int pitch = scaledw * ps;
    YUVImage yuvImage = null;

    if (jpegQual > 0)
      qualStr = new String("_Q" + jpegQual);

    tjd = new TJDecompressor();

    if (dstBuf == null)
      dstBuf = new byte[pitch * scaledh];

    /* Set the destination buffer to gray so we know whether the decompressor
       attempted to write to it */
    Arrays.fill(dstBuf, (byte)127);

    if (doYUV) {
      int width = doTile ? tilew : scaledw;
      int height = doTile ? tileh : scaledh;
      yuvImage = new YUVImage(width, yuvpad, height, subsamp);
      Arrays.fill(yuvImage.getBuf(), (byte)127);
    }

    /* Benchmark */
    iter -= warmup;
    elapsed = elapsedDecode = 0.0;
    while (true) {
      int tile = 0;
      double start = getTime();
      for (int y = 0; y < h; y += tileh) {
        for (int x = 0; x < w; x += tilew, tile++) {
          int width = doTile ? Math.min(tilew, w - x) : scaledw;
          int height = doTile ? Math.min(tileh, h - y) : scaledh;
          tjd.setSourceImage(jpegBuf[tile], jpegSize[tile]);
          if (doYUV) {
            yuvImage.setBuf(yuvImage.getBuf(), width, yuvpad, height, subsamp);
            tjd.decompressToYUV(yuvImage, flags);
            double startDecode = getTime();
            tjd.setSourceImage(yuvImage);
            tjd.decompress(dstBuf, x, y, width, pitch, height, pf, flags);
            if (iter >= 0)
              elapsedDecode += getTime() - startDecode;
          } else
            tjd.decompress(dstBuf, x, y, width, pitch, height, pf, flags);
        }
      }
      iter++;
      if (iter >= 1) {
        elapsed += getTime() - start;
        if (elapsed >= benchTime)
          break;
      }
    }
    if(doYUV)
      elapsed -= elapsedDecode;

    tjd = null;
    for (i = 0; i < jpegBuf.length; i++)
      jpegBuf[i] = null;
    jpegBuf = null;  jpegSize = null;
    System.gc();

    if (quiet != 0) {
      System.out.format("%-6s%s",
        sigFig((double)(w * h) / 1000000. * (double)iter / elapsed, 4),
        quiet == 2 ? "\n" : "  ");
      if (doYUV)
        System.out.format("%s\n",
          sigFig((double)(w * h) / 1000000. * (double)iter / elapsedDecode, 4));
      else if (quiet != 2)
        System.out.print("\n");
    } else {
      System.out.format("%s --> Frame rate:         %f fps\n",
                        (doYUV ? "Decomp to YUV":"Decompress   "),
                        (double)iter / elapsed);
      System.out.format("                  Throughput:         %f Megapixels/sec\n",
                        (double)(w * h) / 1000000. * (double)iter / elapsed);
      if (doYUV) {
        System.out.format("YUV Decode    --> Frame rate:         %f fps\n",
                          (double)iter / elapsedDecode);
        System.out.format("                  Throughput:         %f Megapixels/sec\n",
                          (double)(w * h) / 1000000. * (double)iter / elapsedDecode);
      }
    }

    if (sf.getNum() != 1 || sf.getDenom() != 1)
      sizeStr = new String(sf.getNum() + "_" + sf.getDenom());
    else if (tilew != w || tileh != h)
      sizeStr = new String(tilew + "x" + tileh);
    else
      sizeStr = new String("full");
    if (decompOnly)
      tempStr = new String(fileName + "_" + sizeStr + ".bmp");
    else
      tempStr = new String(fileName + "_" + subName[subsamp] + qualStr +
                           "_" + sizeStr + ".bmp");

    saveImage(tempStr, dstBuf, scaledw, scaledh, pf);
    int ndx = tempStr.lastIndexOf('.');
    tempStr = new String(tempStr.substring(0, ndx) + "-err.bmp");
    if (srcBuf != null && sf.getNum() == 1 && sf.getDenom() == 1) {
      if (quiet == 0)
        System.out.println("Compression error written to " + tempStr + ".");
      if (subsamp == TJ.SAMP_GRAY) {
        for (int y = 0, index = 0; y < h; y++, index += pitch) {
          for (int x = 0, index2 = index; x < w; x++, index2 += ps) {
            int rindex = index2 + TJ.getRedOffset(pf);
            int gindex = index2 + TJ.getGreenOffset(pf);
            int bindex = index2 + TJ.getBlueOffset(pf);
            int lum = (int)((double)(srcBuf[rindex] & 0xff) * 0.299 +
                            (double)(srcBuf[gindex] & 0xff) * 0.587 +
                            (double)(srcBuf[bindex] & 0xff) * 0.114 + 0.5);
            if (lum > 255) lum = 255;
            if (lum < 0) lum = 0;
            dstBuf[rindex] = (byte)Math.abs((dstBuf[rindex] & 0xff) - lum);
            dstBuf[gindex] = (byte)Math.abs((dstBuf[gindex] & 0xff) - lum);
            dstBuf[bindex] = (byte)Math.abs((dstBuf[bindex] & 0xff) - lum);
          }
        }
      } else {
        for (int y = 0; y < h; y++)
          for (int x = 0; x < w * ps; x++)
            dstBuf[pitch * y + x] =
              (byte)Math.abs((dstBuf[pitch * y + x] & 0xff) -
                             (srcBuf[pitch * y + x] & 0xff));
      }
      saveImage(tempStr, dstBuf, w, h, pf);
    }
  }


  static void fullTest(byte[] srcBuf, int w, int h, int subsamp, int jpegQual,
                       String fileName) throws Exception {
    TJCompressor tjc;
    byte[] tmpBuf;
    byte[][] jpegBuf;
    int[] jpegSize;
    double start, elapsed, elapsedEncode;
    int totalJpegSize = 0, tilew, tileh, i, iter;
    int ps = TJ.getPixelSize(pf);
    int ntilesw = 1, ntilesh = 1, pitch = w * ps;
    String pfStr = pixFormatStr[pf];
    YUVImage yuvImage = null;

    tmpBuf = new byte[pitch * h];

    if (quiet == 0)
      System.out.format(">>>>>  %s (%s) <--> JPEG %s Q%d  <<<<<\n", pfStr,
        (flags & TJ.FLAG_BOTTOMUP) != 0 ? "Bottom-up" : "Top-down",
        subNameLong[subsamp], jpegQual);

    tjc = new TJCompressor();

    for (tilew = doTile ? 8 : w, tileh = doTile ? 8 : h; ;
         tilew *= 2, tileh *= 2) {
      if (tilew > w)
        tilew = w;
      if (tileh > h)
        tileh = h;
      ntilesw = (w + tilew - 1) / tilew;
      ntilesh = (h + tileh - 1) / tileh;

      jpegBuf = new byte[ntilesw * ntilesh][TJ.bufSize(tilew, tileh, subsamp)];
      jpegSize = new int[ntilesw * ntilesh];

      /* Compression test */
      if (quiet == 1)
        System.out.format("%-4s (%s)  %-5s    %-3d   ", pfStr,
                          (flags & TJ.FLAG_BOTTOMUP) != 0 ? "BU" : "TD",
                          subNameLong[subsamp], jpegQual);
      for (i = 0; i < h; i++)
        System.arraycopy(srcBuf, w * ps * i, tmpBuf, pitch * i, w * ps);
      tjc.setJPEGQuality(jpegQual);
      tjc.setSubsamp(subsamp);

      if (doYUV) {
        yuvImage = new YUVImage(tilew, yuvpad, tileh, subsamp);
        Arrays.fill(yuvImage.getBuf(), (byte)127);
      }

      /* Benchmark */
      iter = -warmup;
      elapsed = elapsedEncode = 0.0;
      while (true) {
        int tile = 0;
        totalJpegSize = 0;
        start = getTime();
        for (int y = 0; y < h; y += tileh) {
          for (int x = 0; x < w; x += tilew, tile++) {
            int width = Math.min(tilew, w - x);
            int height = Math.min(tileh, h - y);
            tjc.setSourceImage(srcBuf, x, y, width, pitch, height, pf);
            if (doYUV) {
              double startEncode = getTime();
              yuvImage.setBuf(yuvImage.getBuf(), width, yuvpad, height,
                              subsamp);
              tjc.encodeYUV(yuvImage, flags);
              if (iter >= 0)
                elapsedEncode += getTime() - startEncode;
              tjc.setSourceImage(yuvImage);
            }
            tjc.compress(jpegBuf[tile], flags);
            jpegSize[tile] = tjc.getCompressedSize();
            totalJpegSize += jpegSize[tile];
          }
        }
        iter++;
        if (iter >= 1) {
          elapsed += getTime() - start;
          if (elapsed >= benchTime)
            break;
        }
      }
      if (doYUV)
        elapsed -= elapsedEncode;

      if (quiet == 1)
        System.out.format("%-5d  %-5d   ", tilew, tileh);
      if (quiet != 0) {
        if (doYUV)
          System.out.format("%-6s%s",
            sigFig((double)(w * h) / 1000000. * (double)iter / elapsedEncode, 4),
            quiet == 2 ? "\n" : "  ");
        System.out.format("%-6s%s",
          sigFig((double)(w * h) / 1000000. * (double)iter / elapsed, 4),
          quiet == 2 ? "\n" : "  ");
        System.out.format("%-6s%s",
          sigFig((double)(w * h * ps) / (double)totalJpegSize, 4),
          quiet == 2 ? "\n" : "  ");
      } else {
        System.out.format("\n%s size: %d x %d\n", doTile ? "Tile" : "Image",
                          tilew, tileh);
        if (doYUV) {
          System.out.format("Encode YUV    --> Frame rate:         %f fps\n",
                            (double)iter / elapsedEncode);
          System.out.format("                  Output image size:  %d bytes\n",
                            yuvImage.getSize());
          System.out.format("                  Compression ratio:  %f:1\n",
                            (double)(w * h * ps) / (double)yuvImage.getSize());
          System.out.format("                  Throughput:         %f Megapixels/sec\n",
                            (double)(w * h) / 1000000. * (double)iter / elapsedEncode);
          System.out.format("                  Output bit stream:  %f Megabits/sec\n",
            (double)yuvImage.getSize() * 8. / 1000000. * (double)iter / elapsedEncode);
        }
        System.out.format("%s --> Frame rate:         %f fps\n",
                          doYUV ? "Comp from YUV" : "Compress     ",
                          (double)iter / elapsed);
        System.out.format("                  Output image size:  %d bytes\n",
                          totalJpegSize);
        System.out.format("                  Compression ratio:  %f:1\n",
                          (double)(w * h * ps) / (double)totalJpegSize);
        System.out.format("                  Throughput:         %f Megapixels/sec\n",
                          (double)(w * h) / 1000000. * (double)iter / elapsed);
        System.out.format("                  Output bit stream:  %f Megabits/sec\n",
          (double)totalJpegSize * 8. / 1000000. * (double)iter / elapsed);
      }
      if (tilew == w && tileh == h) {
        String tempStr = fileName + "_" + subName[subsamp] + "_" + "Q" +
                         jpegQual + ".jpg";
        FileOutputStream fos = new FileOutputStream(tempStr);
        fos.write(jpegBuf[0], 0, jpegSize[0]);
        fos.close();
        if (quiet == 0)
          System.out.println("Reference image written to " + tempStr);
      }

      /* Decompression test */
      if (!compOnly)
        decomp(srcBuf, jpegBuf, jpegSize, tmpBuf, w, h, subsamp, jpegQual,
               fileName, tilew, tileh);

      if (tilew == w && tileh == h) break;
    }
  }


  static void decompTest(String fileName) throws Exception {
    TJTransformer tjt;
    byte[][] jpegBuf = null;
    byte[] srcBuf;
    int[] jpegSize = null;
    int totalJpegSize;
    int w = 0, h = 0, subsamp = -1, cs = -1, _w, _h, _tilew, _tileh,
      _ntilesw, _ntilesh, _subsamp, x, y, iter;
    int ntilesw = 1, ntilesh = 1;
    double start, elapsed;
    int ps = TJ.getPixelSize(pf), tile;

    FileInputStream fis = new FileInputStream(fileName);
    int srcSize = (int)fis.getChannel().size();
    srcBuf = new byte[srcSize];
    fis.read(srcBuf, 0, srcSize);
    fis.close();

    int index = fileName.lastIndexOf('.');
    if (index >= 0)
      fileName = new String(fileName.substring(0, index));

    tjt = new TJTransformer();

    tjt.setSourceImage(srcBuf, srcSize);
    w = tjt.getWidth();
    h = tjt.getHeight();
    subsamp = tjt.getSubsamp();
    cs = tjt.getColorspace();

    if (quiet == 1) {
      System.out.println("All performance values in Mpixels/sec\n");
      System.out.format("Bitmap     JPEG   JPEG     %s  %s   Xform   Comp    Decomp  ",
                        (doTile ? "Tile " : "Image"),
                        (doTile ? "Tile " : "Image"));
      if (doYUV)
        System.out.print("Decode");
      System.out.print("\n");
      System.out.print("Format     CS     Subsamp  Width  Height  Perf    Ratio   Perf    ");
      if (doYUV)
        System.out.print("Perf");
      System.out.println("\n");
    } else if (quiet == 0)
      System.out.format(">>>>>  JPEG %s --> %s (%s)  <<<<<\n",
        formatName(subsamp, cs), pixFormatStr[pf],
        (flags & TJ.FLAG_BOTTOMUP) != 0 ? "Bottom-up" : "Top-down");

    for (int tilew = doTile ? 16 : w, tileh = doTile ? 16 : h; ;
         tilew *= 2, tileh *= 2) {
      if (tilew > w)
        tilew = w;
      if (tileh > h)
        tileh = h;
      ntilesw = (w + tilew - 1) / tilew;
      ntilesh = (h + tileh - 1) / tileh;

      _w = w;  _h = h;  _tilew = tilew;  _tileh = tileh;
      if (quiet == 0) {
        System.out.format("\n%s size: %d x %d", (doTile ? "Tile" : "Image"),
                          _tilew, _tileh);
        if (sf.getNum() != 1 || sf.getDenom() != 1)
          System.out.format(" --> %d x %d", sf.getScaled(_w),
                            sf.getScaled(_h));
        System.out.println("");
      } else if (quiet == 1) {
        System.out.format("%-4s (%s)  %-5s  %-5s    ", pixFormatStr[pf],
                          (flags & TJ.FLAG_BOTTOMUP) != 0 ? "BU" : "TD",
                          csName[cs], subNameLong[subsamp]);
        System.out.format("%-5d  %-5d   ", tilew, tileh);
      }

      _subsamp = subsamp;
      if (doTile || xformOp != TJTransform.OP_NONE || xformOpt != 0) {
        if (xformOp == TJTransform.OP_TRANSPOSE ||
            xformOp == TJTransform.OP_TRANSVERSE ||
            xformOp == TJTransform.OP_ROT90 ||
            xformOp == TJTransform.OP_ROT270) {
          _w = h;  _h = w;  _tilew = tileh;  _tileh = tilew;
        }

        if ((xformOpt & TJTransform.OPT_GRAY) != 0)
          _subsamp = TJ.SAMP_GRAY;
        if (xformOp == TJTransform.OP_HFLIP ||
            xformOp == TJTransform.OP_ROT180)
          _w = _w - (_w % TJ.getMCUWidth(_subsamp));
        if (xformOp == TJTransform.OP_VFLIP ||
            xformOp == TJTransform.OP_ROT180)
          _h = _h - (_h % TJ.getMCUHeight(_subsamp));
        if (xformOp == TJTransform.OP_TRANSVERSE ||
            xformOp == TJTransform.OP_ROT90)
          _w = _w - (_w % TJ.getMCUHeight(_subsamp));
        if (xformOp == TJTransform.OP_TRANSVERSE ||
            xformOp == TJTransform.OP_ROT270)
          _h = _h - (_h % TJ.getMCUWidth(_subsamp));
        _ntilesw = (_w + _tilew - 1) / _tilew;
        _ntilesh = (_h + _tileh - 1) / _tileh;

        if (xformOp == TJTransform.OP_TRANSPOSE ||
            xformOp == TJTransform.OP_TRANSVERSE ||
            xformOp == TJTransform.OP_ROT90 ||
            xformOp == TJTransform.OP_ROT270) {
            if (_subsamp == TJ.SAMP_422)
              _subsamp = TJ.SAMP_440;
            else if (_subsamp == TJ.SAMP_440)
              _subsamp = TJ.SAMP_422;
        }

        TJTransform[] t = new TJTransform[_ntilesw * _ntilesh];
        jpegBuf = new byte[_ntilesw * _ntilesh][TJ.bufSize(_tilew, _tileh, subsamp)];

        for (y = 0, tile = 0; y < _h; y += _tileh) {
          for (x = 0; x < _w; x += _tilew, tile++) {
            t[tile] = new TJTransform();
            t[tile].width = Math.min(_tilew, _w - x);
            t[tile].height = Math.min(_tileh, _h - y);
            t[tile].x = x;
            t[tile].y = y;
            t[tile].op = xformOp;
            t[tile].options = xformOpt | TJTransform.OPT_TRIM;
            if ((t[tile].options & TJTransform.OPT_NOOUTPUT) != 0 &&
                jpegBuf[tile] != null)
              jpegBuf[tile] = null;
          }
        }

        iter = -warmup;
        elapsed = 0.;
        while (true) {
          start = getTime();
          tjt.transform(jpegBuf, t, flags);
          jpegSize = tjt.getTransformedSizes();
          iter++;
          if (iter >= 1) {
            elapsed += getTime() - start;
            if (elapsed >= benchTime)
              break;
          }
        }
        t = null;

        for (tile = 0, totalJpegSize = 0; tile < _ntilesw * _ntilesh; tile++)
          totalJpegSize += jpegSize[tile];

        if (quiet != 0) {
          System.out.format("%-6s%s%-6s%s",
            sigFig((double)(w * h) / 1000000. / elapsed, 4),
            quiet == 2 ? "\n" : "  ",
            sigFig((double)(w * h * ps) / (double)totalJpegSize, 4),
            quiet == 2 ? "\n" : "  ");
        } else if (quiet == 0) {
          System.out.format("Transform     --> Frame rate:         %f fps\n",
                            1.0 / elapsed);
          System.out.format("                  Output image size:  %d bytes\n",
                            totalJpegSize);
          System.out.format("                  Compression ratio:  %f:1\n",
                            (double)(w * h * ps) / (double)totalJpegSize);
          System.out.format("                  Throughput:         %f Megapixels/sec\n",
                            (double)(w * h) / 1000000. / elapsed);
          System.out.format("                  Output bit stream:  %f Megabits/sec\n",
                            (double)totalJpegSize * 8. / 1000000. / elapsed);
        }
      } else {
        if (quiet == 1)
          System.out.print("N/A     N/A     ");
        jpegBuf = new byte[1][TJ.bufSize(_tilew, _tileh, subsamp)];
        jpegSize = new int[1];
        jpegSize[0] = srcSize;
        System.arraycopy(srcBuf, 0, jpegBuf[0], 0, srcSize);
      }

      if (w == tilew)
        _tilew = _w;
      if (h == tileh)
        _tileh = _h;
      if ((xformOpt & TJTransform.OPT_NOOUTPUT) == 0)
        decomp(null, jpegBuf, jpegSize, null, _w, _h, _subsamp, 0,
               fileName, _tilew, _tileh);
      else if (quiet == 1)
        System.out.println("N/A");

      jpegBuf = null;
      jpegSize = null;

      if (tilew == w && tileh == h) break;
    }
  }


  static void usage() throws Exception {
    int i;
    TJScalingFactor[] scalingFactors = TJ.getScalingFactors();
    int nsf = scalingFactors.length;
    String className = new TJBench().getClass().getName();

    System.out.println("\nUSAGE: java " + className);
    System.out.println("       <Inputfile (BMP)> <Quality> [options]\n");
    System.out.println("       java " + className);
    System.out.println("       <Inputfile (JPG)> [options]\n");
    System.out.println("Options:\n");
    System.out.println("-alloc = Dynamically allocate JPEG image buffers");
    System.out.println("-bottomup = Test bottom-up compression/decompression");
    System.out.println("-tile = Test performance of the codec when the image is encoded as separate");
    System.out.println("     tiles of varying sizes.");
    System.out.println("-rgb, -bgr, -rgbx, -bgrx, -xbgr, -xrgb =");
    System.out.println("     Test the specified color conversion path in the codec (default = BGR)");
    System.out.println("-fastupsample = Use the fastest chrominance upsampling algorithm available in");
    System.out.println("     the underlying codec");
    System.out.println("-fastdct = Use the fastest DCT/IDCT algorithms available in the underlying");
    System.out.println("     codec");
    System.out.println("-accuratedct = Use the most accurate DCT/IDCT algorithms available in the");
    System.out.println("     underlying codec");
    System.out.println("-subsamp <s> = When testing JPEG compression, this option specifies the level");
    System.out.println("     of chrominance subsampling to use (<s> = 444, 422, 440, 420, 411, or");
    System.out.println("     GRAY).  The default is to test Grayscale, 4:2:0, 4:2:2, and 4:4:4 in");
    System.out.println("     sequence.");
    System.out.println("-quiet = Output results in tabular rather than verbose format");
    System.out.println("-yuv = Test YUV encoding/decoding functions");
    System.out.println("-yuvpad <p> = If testing YUV encoding/decoding, this specifies the number of");
    System.out.println("     bytes to which each row of each plane in the intermediate YUV image is");
    System.out.println("     padded (default = 1)");
    System.out.println("-scale M/N = Scale down the width/height of the decompressed JPEG image by a");
    System.out.print  ("     factor of M/N (M/N = ");
    for (i = 0; i < nsf; i++) {
      System.out.format("%d/%d", scalingFactors[i].getNum(),
                        scalingFactors[i].getDenom());
      if (nsf == 2 && i != nsf - 1)
        System.out.print(" or ");
      else if (nsf > 2) {
        if (i != nsf - 1)
          System.out.print(", ");
        if (i == nsf - 2)
          System.out.print("or ");
      }
      if (i % 8 == 0 && i != 0)
        System.out.print("\n     ");
    }
    System.out.println(")");
    System.out.println("-hflip, -vflip, -transpose, -transverse, -rot90, -rot180, -rot270 =");
    System.out.println("     Perform the corresponding lossless transform prior to");
    System.out.println("     decompression (these options are mutually exclusive)");
    System.out.println("-grayscale = Perform lossless grayscale conversion prior to decompression");
    System.out.println("     test (can be combined with the other transforms above)");
    System.out.println("-benchtime <t> = Run each benchmark for at least <t> seconds (default = 5.0)");
    System.out.println("-warmup <w> = Execute each benchmark <w> times to prime the cache before");
    System.out.println("     taking performance measurements (default = 1)");
    System.out.println("-componly = Stop after running compression tests.  Do not test decompression.\n");
    System.out.println("NOTE:  If the quality is specified as a range (e.g. 90-100), a separate");
    System.out.println("test will be performed for all quality values in the range.\n");
    System.exit(1);
  }


  public static void main(String[] argv) {
    byte[] srcBuf = null;  int w = 0, h = 0;
    int minQual = -1, maxQual = -1;
    int minArg = 1;  int retval = 0;
    int subsamp = -1;

    try {

      if (argv.length < minArg)
        usage();

      String tempStr = argv[0].toLowerCase();
      if (tempStr.endsWith(".jpg") || tempStr.endsWith(".jpeg"))
        decompOnly = true;

      System.out.println("");

      if (!decompOnly) {
        minArg = 2;
        if (argv.length < minArg)
          usage();
        try {
          minQual = Integer.parseInt(argv[1]);
        } catch (NumberFormatException e) {}
        if (minQual < 1 || minQual > 100)
          throw new Exception("Quality must be between 1 and 100.");
        int dashIndex = argv[1].indexOf('-');
        if (dashIndex > 0 && argv[1].length() > dashIndex + 1) {
          try {
            maxQual = Integer.parseInt(argv[1].substring(dashIndex + 1));
          } catch (NumberFormatException e) {}
        }
        if (maxQual < 1 || maxQual > 100)
          maxQual = minQual;
      }

      if (argv.length > minArg) {
        for (int i = minArg; i < argv.length; i++) {
          if (argv[i].equalsIgnoreCase("-tile")) {
            doTile = true;  xformOpt |= TJTransform.OPT_CROP;
          }
          if (argv[i].equalsIgnoreCase("-fastupsample")) {
            System.out.println("Using fast upsampling code\n");
            flags |= TJ.FLAG_FASTUPSAMPLE;
          }
          if (argv[i].equalsIgnoreCase("-fastdct")) {
            System.out.println("Using fastest DCT/IDCT algorithm\n");
            flags |= TJ.FLAG_FASTDCT;
          }
          if (argv[i].equalsIgnoreCase("-accuratedct")) {
            System.out.println("Using most accurate DCT/IDCT algorithm\n");
            flags |= TJ.FLAG_ACCURATEDCT;
          }
          if (argv[i].equalsIgnoreCase("-rgb"))
            pf = TJ.PF_RGB;
          if (argv[i].equalsIgnoreCase("-rgbx"))
            pf = TJ.PF_RGBX;
          if (argv[i].equalsIgnoreCase("-bgr"))
            pf = TJ.PF_BGR;
          if (argv[i].equalsIgnoreCase("-bgrx"))
            pf = TJ.PF_BGRX;
          if (argv[i].equalsIgnoreCase("-xbgr"))
            pf = TJ.PF_XBGR;
          if (argv[i].equalsIgnoreCase("-xrgb"))
            pf = TJ.PF_XRGB;
          if (argv[i].equalsIgnoreCase("-bottomup"))
            flags |= TJ.FLAG_BOTTOMUP;
          if (argv[i].equalsIgnoreCase("-quiet"))
            quiet = 1;
          if (argv[i].equalsIgnoreCase("-qq"))
            quiet = 2;
          if (argv[i].equalsIgnoreCase("-scale") && i < argv.length - 1) {
            int temp1 = 0, temp2 = 0;
            boolean match = false, scanned = true;
            Scanner scanner = new Scanner(argv[++i]).useDelimiter("/");
            try {
              temp1 = scanner.nextInt();
              temp2 = scanner.nextInt();
            } catch(Exception e) {}
            if (temp2 <= 0) temp2 = 1;
            if (temp1 > 0) {
              TJScalingFactor[] scalingFactors = TJ.getScalingFactors();
              for (int j = 0; j < scalingFactors.length; j++) {
                if ((double)temp1 / (double)temp2 ==
                    (double)scalingFactors[j].getNum() /
                    (double)scalingFactors[j].getDenom()) {
                  sf = scalingFactors[j];
                  match = true;   break;
                }
              }
              if (!match) usage();
            } else
              usage();
          }
          if (argv[i].equalsIgnoreCase("-hflip"))
            xformOp = TJTransform.OP_HFLIP;
          if (argv[i].equalsIgnoreCase("-vflip"))
            xformOp = TJTransform.OP_VFLIP;
          if (argv[i].equalsIgnoreCase("-transpose"))
            xformOp = TJTransform.OP_TRANSPOSE;
          if (argv[i].equalsIgnoreCase("-transverse"))
            xformOp = TJTransform.OP_TRANSVERSE;
          if (argv[i].equalsIgnoreCase("-rot90"))
            xformOp = TJTransform.OP_ROT90;
          if (argv[i].equalsIgnoreCase("-rot180"))
            xformOp = TJTransform.OP_ROT180;
          if (argv[i].equalsIgnoreCase("-rot270"))
            xformOp = TJTransform.OP_ROT270;
          if (argv[i].equalsIgnoreCase("-grayscale"))
            xformOpt |= TJTransform.OPT_GRAY;
          if (argv[i].equalsIgnoreCase("-nooutput"))
            xformOpt |= TJTransform.OPT_NOOUTPUT;
          if (argv[i].equalsIgnoreCase("-benchtime") && i < argv.length - 1) {
            double temp = -1;
            try {
              temp = Double.parseDouble(argv[++i]);
            } catch (NumberFormatException e) {}
            if (temp > 0.0)
              benchTime = temp;
            else
              usage();
          }
          if (argv[i].equalsIgnoreCase("-yuv")) {
            System.out.println("Testing YUV planar encoding/decoding\n");
            doYUV = true;
          }
          if (argv[i].equalsIgnoreCase("-yuvpad") && i < argv.length - 1) {
            int temp = 0;
            try {
             temp = Integer.parseInt(argv[++i]);
            } catch (NumberFormatException e) {}
            if (temp >= 1)
              yuvpad = temp;
          }
          if (argv[i].equalsIgnoreCase("-subsamp") && i < argv.length - 1) {
            i++;
            if (argv[i].toUpperCase().startsWith("G"))
              subsamp = TJ.SAMP_GRAY;
            else if (argv[i].equals("444"))
              subsamp = TJ.SAMP_444;
            else if (argv[i].equals("422"))
              subsamp = TJ.SAMP_422;
            else if (argv[i].equals("440"))
              subsamp = TJ.SAMP_440;
            else if (argv[i].equals("420"))
              subsamp = TJ.SAMP_420;
            else if (argv[i].equals("411"))
              subsamp = TJ.SAMP_411;
          }
          if (argv[i].equalsIgnoreCase("-componly"))
            compOnly = true;
          if (argv[i].equalsIgnoreCase("-warmup") && i < argv.length - 1) {
            int temp = -1;
            try {
             temp = Integer.parseInt(argv[++i]);
            } catch (NumberFormatException e) {}
            if (temp >= 0) {
              warmup = temp;
              System.out.format("Warmup runs = %d\n\n", warmup);
            }
          }
          if (argv[i].equalsIgnoreCase("-?"))
            usage();
        }
      }

      if (sf == null)
        sf = new TJScalingFactor(1, 1);

      if ((sf.getNum() != 1 || sf.getDenom() != 1) && doTile) {
        System.out.println("Disabling tiled compression/decompression tests, because those tests do not");
        System.out.println("work when scaled decompression is enabled.");
        doTile = false;
      }

      if (!decompOnly) {
        int[] width = new int[1], height = new int[1];
        srcBuf = loadImage(argv[0], width, height, pf);
        w = width[0];  h = height[0];
        int index = -1;
        if ((index = argv[0].lastIndexOf('.')) >= 0)
          argv[0] = argv[0].substring(0, index);
      }

      if (quiet == 1 && !decompOnly) {
        System.out.println("All performance values in Mpixels/sec\n");
        System.out.format("Bitmap     JPEG     JPEG  %s  %s   ",
          (doTile ? "Tile " : "Image"), (doTile ? "Tile " : "Image"));
        if (doYUV)
          System.out.print("Encode  ");
        System.out.print("Comp    Comp    Decomp  ");
        if (doYUV)
          System.out.print("Decode");
        System.out.print("\n");
        System.out.print("Format     Subsamp  Qual  Width  Height  ");
        if (doYUV)
          System.out.print("Perf    ");
        System.out.print("Perf    Ratio   Perf    ");
        if (doYUV)
          System.out.print("Perf");
        System.out.println("\n");
      }

      if (decompOnly) {
        decompTest(argv[0]);
        System.out.println("");
        System.exit(retval);
      }

      System.gc();
      if (subsamp >= 0 && subsamp < TJ.NUMSAMP) {
        for (int i = maxQual; i >= minQual; i--)
          fullTest(srcBuf, w, h, subsamp, i, argv[0]);
        System.out.println("");
      } else {
        for (int i = maxQual; i >= minQual; i--)
          fullTest(srcBuf, w, h, TJ.SAMP_GRAY, i, argv[0]);
        System.out.println("");
        System.gc();
        for (int i = maxQual; i >= minQual; i--)
          fullTest(srcBuf, w, h, TJ.SAMP_420, i, argv[0]);
        System.out.println("");
        System.gc();
        for (int i = maxQual; i >= minQual; i--)
          fullTest(srcBuf, w, h, TJ.SAMP_422, i, argv[0]);
        System.out.println("");
        System.gc();
        for (int i = maxQual; i >= minQual; i--)
          fullTest(srcBuf, w, h, TJ.SAMP_444, i, argv[0]);
        System.out.println("");
      }

    } catch (Exception e) {
      System.out.println("ERROR: " + e.getMessage());
      e.printStackTrace();
      retval = -1;
    }

    System.exit(retval);
  }

}
