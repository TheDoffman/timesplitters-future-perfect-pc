using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;

// Build-time vector artwork, rasterized independently at each Windows icon size.
// Keep the wide title lockup for the sidebar; a compact mark survives at 16px.
public static class LauncherIconBuilder
{
    static readonly int[] Sizes = { 16, 24, 32, 48, 64, 128, 256 };

    static GraphicsPath Polygon(params float[] coordinates)
    {
        var points = new PointF[coordinates.Length / 2];
        for (int i = 0; i < points.Length; i++)
            points[i] = new PointF(coordinates[i * 2], coordinates[i * 2 + 1]);
        var path = new GraphicsPath();
        path.AddPolygon(points);
        return path;
    }

    static Bitmap Render(int size)
    {
        var bitmap = new Bitmap(size, size, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(bitmap))
        {
            g.Clear(Color.Transparent);
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.PixelOffsetMode = PixelOffsetMode.HighQuality;
            g.ScaleTransform(size / 256f, size / 256f);

            using (var rim = new LinearGradientBrush(new Point(30, 12), new Point(220, 244),
                Color.FromArgb(225, 244, 255), Color.FromArgb(48, 91, 131)))
                g.FillEllipse(rim, 9, 9, 238, 238);
            using (var face = new LinearGradientBrush(new Point(60, 24), new Point(180, 232),
                Color.FromArgb(29, 63, 94), Color.FromArgb(7, 18, 33)))
                g.FillEllipse(face, 18, 18, 220, 220);

            // Interrupted rings suggest a temporal aperture rather than a clock.
            using (var glow = new Pen(Color.FromArgb(55, 50, 165, 255), 19))
            using (var blue = new Pen(Color.FromArgb(104, 205, 255), size <= 24 ? 11 : 7))
            using (var silver = new Pen(Color.FromArgb(209, 235, 252), 4))
            {
                g.DrawArc(glow, 31, 31, 194, 194, 194, 137);
                g.DrawArc(glow, 31, 31, 194, 194, 14, 137);
                g.DrawArc(blue, 31, 31, 194, 194, 194, 137);
                g.DrawArc(blue, 31, 31, 194, 194, 14, 137);
                if (size >= 32)
                {
                    g.DrawArc(silver, 23, 23, 210, 210, 205, 72);
                    g.DrawArc(silver, 23, 23, 210, 210, 25, 72);
                }
            }

            using (var t = Polygon(56, 86, 130, 86, 126, 107, 105, 107,
                105, 171, 80, 178, 80, 107, 53, 107))
            using (var s = Polygon(151, 86, 198, 86, 194, 107, 155, 107,
                155, 119, 190, 119, 201, 131, 196, 160, 182, 173,
                123, 173, 128, 151, 173, 151, 176, 140, 142, 140,
                131, 128, 136, 99))
            using (var chrome = new LinearGradientBrush(new Point(0, 85), new Point(0, 178),
                Color.White, Color.SteelBlue))
            using (var outline = new Pen(Color.FromArgb(7, 19, 33), 5))
            {
                chrome.InterpolationColors = new ColorBlend {
                    Colors = new[] { Color.FromArgb(247, 252, 255), Color.FromArgb(195, 220, 239),
                        Color.FromArgb(114, 151, 181), Color.FromArgb(230, 244, 254) },
                    Positions = new[] { 0f, 0.46f, 0.51f, 1f }
                };
                g.DrawPath(outline, t); g.DrawPath(outline, s);
                g.FillPath(chrome, t); g.FillPath(chrome, s);
            }
        }
        return bitmap;
    }

    public static void Write(string output)
    {
        var images = new byte[Sizes.Length][];
        for (int i = 0; i < Sizes.Length; i++)
        {
            using (var bitmap = Render(Sizes[i]))
            using (var stream = new MemoryStream())
            {
                bitmap.Save(stream, ImageFormat.Png);
                images[i] = stream.ToArray();
            }
        }
        using (var writer = new BinaryWriter(File.Create(output)))
        {
            writer.Write((ushort)0); writer.Write((ushort)1); writer.Write((ushort)Sizes.Length);
            int offset = 6 + 16 * Sizes.Length;
            for (int i = 0; i < Sizes.Length; i++)
            {
                byte dimension = (byte)(Sizes[i] == 256 ? 0 : Sizes[i]);
                writer.Write(dimension); writer.Write(dimension);
                writer.Write((byte)0); writer.Write((byte)0);
                writer.Write((ushort)1); writer.Write((ushort)32);
                writer.Write(images[i].Length); writer.Write(offset);
                offset += images[i].Length;
            }
            foreach (var image in images) writer.Write(image);
        }
    }

    public static void Preview(string output)
    {
        using (var sheet = new Bitmap(850, 320))
        using (var g = Graphics.FromImage(sheet))
        using (var font = new Font("Segoe UI", 10))
        using (var label = new SolidBrush(Color.FromArgb(195, 216, 234)))
        {
            g.Clear(Color.FromArgb(11, 20, 32));
            int x = 20;
            foreach (int size in Sizes)
            {
                using (var icon = Render(size)) g.DrawImageUnscaled(icon, x, (280 - size) / 2);
                g.DrawString(size + " px", font, label, x, 286);
                x += Math.Max(size + 18, 52);
            }
            sheet.Save(output, ImageFormat.Png);
        }
    }
}
