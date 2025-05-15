var markdownpdf = require("markdown-pdf")
  , fs = require("fs")

var mdOptions = {
  cssPath: "misc/gendocs/style.css"
}

markdownpdf(mdOptions).from("docs/manual.md").to("docs/manual.pdf", function () {
  console.log("Manual PDF generated");
})
