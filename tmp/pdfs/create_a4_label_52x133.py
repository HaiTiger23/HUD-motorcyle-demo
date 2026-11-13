from pathlib import Path

from reportlab.lib.pagesizes import A4, landscape
from reportlab.lib.units import cm
from reportlab.pdfbase.pdfmetrics import stringWidth
from reportlab.pdfgen import canvas


ROOT = Path("/Users/macbook/Project/HUD-motorcyle-demo")
SOURCE = ROOT / "output" / "imagegen" / "nhan-thuoc-da-lam-net.png"
DESTINATION = ROOT / "output" / "pdf" / "nhan-thuoc-a4-5_2x13_3cm.pdf"
CAPTION = "5,2 x 13,3 cm"

# The sample's notation is height x width.  The supplied label is landscape.
IMAGE_WIDTH = 13.3 * cm
IMAGE_HEIGHT = 5.2 * cm


def main() -> None:
    page_width, page_height = landscape(A4)
    image_x = (page_width - IMAGE_WIDTH) / 2
    image_y = (page_height - IMAGE_HEIGHT) / 2

    DESTINATION.parent.mkdir(parents=True, exist_ok=True)
    pdf = canvas.Canvas(str(DESTINATION), pagesize=(page_width, page_height))
    pdf.setTitle("Nhan thuoc A4 5,2 x 13,3 cm")
    pdf.setAuthor("Codex")
    pdf.drawImage(
        str(SOURCE), image_x, image_y,
        width=IMAGE_WIDTH, height=IMAGE_HEIGHT,
        preserveAspectRatio=False, mask="auto",
    )
    pdf.setFont("Helvetica", 16)
    caption_x = (page_width - stringWidth(CAPTION, "Helvetica", 16)) / 2
    pdf.drawString(caption_x, image_y + IMAGE_HEIGHT + (0.45 * cm), CAPTION)
    pdf.showPage()
    pdf.save()


if __name__ == "__main__":
    main()
