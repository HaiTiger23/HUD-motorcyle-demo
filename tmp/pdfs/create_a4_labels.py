from pathlib import Path

from reportlab.lib.pagesizes import A4, landscape
from reportlab.lib.units import cm
from reportlab.pdfbase.pdfmetrics import stringWidth
from reportlab.pdfgen import canvas


ROOT = Path("/Users/macbook/Project/HUD-motorcyle-demo")
OUTPUT = ROOT / "output" / "pdf"
CAPTION = "9 x 16 cm"
IMAGE_WIDTH = 16 * cm
IMAGE_HEIGHT = 9 * cm

SOURCES = [
    (
        Path("/Users/macbook/Downloads/1794303448965_7899862445118486093_7899862445118486093_9483c8e05ec60b2a9091368ca6d57e84.jpg"),
        OUTPUT / "nhan-thuoc-1-a4-9x16cm.pdf",
    ),
    (
        Path("/Users/macbook/Downloads/1794361921045_7899862445118486093_7899862445118486093_67309c8b2c4bc0618052c37b25671d55.jpg"),
        OUTPUT / "nhan-thuoc-2-a4-9x16cm.pdf",
    ),
]


def build_pdf(source: Path, destination: Path) -> None:
    """Create one A4 landscape label sheet with an exact 9 cm x 16 cm image."""
    page_width, page_height = landscape(A4)
    image_x = (page_width - IMAGE_WIDTH) / 2
    image_y = (page_height - IMAGE_HEIGHT) / 2

    pdf = canvas.Canvas(str(destination), pagesize=(page_width, page_height))
    pdf.setTitle(destination.stem)
    pdf.setAuthor("Codex")

    # The source is deliberately resized to the requested physical dimensions.
    pdf.drawImage(
        str(source), image_x, image_y, width=IMAGE_WIDTH, height=IMAGE_HEIGHT,
        preserveAspectRatio=False, mask="auto",
    )
    pdf.setFont("Helvetica", 16)
    caption_x = (page_width - stringWidth(CAPTION, "Helvetica", 16)) / 2
    pdf.drawString(caption_x, image_y + IMAGE_HEIGHT + (0.45 * cm), CAPTION)
    pdf.showPage()
    pdf.save()


OUTPUT.mkdir(parents=True, exist_ok=True)
for source, destination in SOURCES:
    build_pdf(source, destination)
