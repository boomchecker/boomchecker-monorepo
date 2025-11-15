package templates

import (
	"embed"
	"fmt"
	"html/template"
	text_template "text/template"
	"strings"
	"time"
)

//go:embed emails/*.html
var htmlTemplates embed.FS

//go:embed emails/*.txt
var textTemplates embed.FS

// TemplateRenderer manages loading and rendering of email templates
type TemplateRenderer struct {
	htmlTemplates *template.Template
	textTemplates *text_template.Template
}

// AdminTokenData holds data for admin token email template
type AdminTokenData struct {
	Token               string
	ExpiresInHours      int
	ExpiresAtFormatted  string
}

// NewTemplateRenderer creates a new template renderer
func NewTemplateRenderer() (*TemplateRenderer, error) {
	// Load HTML templates
	htmlTmpl, err := template.ParseFS(htmlTemplates, "emails/*.html")
	if err != nil {
		return nil, fmt.Errorf("failed to load HTML templates: %w", err)
	}

	// Load text templates
	textTmpl, err := text_template.ParseFS(textTemplates, "emails/*.txt")
	if err != nil {
		return nil, fmt.Errorf("failed to load text templates: %w", err)
	}

	return &TemplateRenderer{
		htmlTemplates: htmlTmpl,
		textTemplates: textTmpl,
	}, nil
}

// RenderAdminTokenHTML renders the HTML email for admin token
func (t *TemplateRenderer) RenderAdminTokenHTML(token string, expiresAt time.Time) (string, error) {
	data := AdminTokenData{
		Token:              token,
		ExpiresInHours:     int(time.Until(expiresAt).Hours()),
		ExpiresAtFormatted: expiresAt.Format("2006-01-02 15:04:05 MST"),
	}

	var buf strings.Builder
	if err := t.htmlTemplates.ExecuteTemplate(&buf, "admin_token.html", data); err != nil {
		return "", fmt.Errorf("failed to render HTML template: %w", err)
	}

	return buf.String(), nil
}

// RenderAdminTokenText renders the text email for admin token
func (t *TemplateRenderer) RenderAdminTokenText(token string, expiresAt time.Time) (string, error) {
	data := AdminTokenData{
		Token:              token,
		ExpiresInHours:     int(time.Until(expiresAt).Hours()),
		ExpiresAtFormatted: expiresAt.Format("2006-01-02 15:04:05 MST"),
	}

	var buf strings.Builder
	if err := t.textTemplates.ExecuteTemplate(&buf, "admin_token.txt", data); err != nil {
		return "", fmt.Errorf("failed to render text template: %w", err)
	}

	return buf.String(), nil
}
