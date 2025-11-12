package services

import (
	"context"
	"fmt"
	"log"
	"time"

	"github.com/aws/aws-sdk-go-v2/aws"
	"github.com/aws/aws-sdk-go-v2/config"
	"github.com/aws/aws-sdk-go-v2/service/sesv2"
	"github.com/aws/aws-sdk-go-v2/service/sesv2/types"

	"github.com/boomchecker/api-backend/internal/templates"
)

// EmailService handles email sending via AWS SES
type EmailService struct {
	client    *sesv2.Client
	fromEmail string
	templates *templates.TemplateRenderer
}

// EmailConfig holds configuration for email service
type EmailConfig struct {
	// FromEmail is the email address that will appear in the From field
	FromEmail string
	// Region is the AWS region for SES (e.g., "us-east-1", "eu-west-1")
	Region string
}

// NewEmailService creates a new email service instance
func NewEmailService(cfg *EmailConfig) (*EmailService, error) {
	if cfg == nil {
		return nil, fmt.Errorf("email config is required")
	}
	if cfg.FromEmail == "" {
		return nil, fmt.Errorf("from email is required")
	}

	// Load AWS configuration with default credentials provider chain
	// This will check: Environment variables -> Shared config file -> IAM role (on EC2)
	awsCfg, err := config.LoadDefaultConfig(context.TODO(),
		config.WithRegion(cfg.Region),
	)
	if err != nil {
		return nil, fmt.Errorf("failed to load AWS config: %w", err)
	}

	// Create SES v2 client
	client := sesv2.NewFromConfig(awsCfg)

	// Load email templates
	tmplRenderer, err := templates.NewTemplateRenderer()
	if err != nil {
		return nil, fmt.Errorf("failed to initialize templates: %w", err)
	}

	return &EmailService{
		client:    client,
		fromEmail: cfg.FromEmail,
		templates: tmplRenderer,
	}, nil
}

// SendAdminToken sends an admin authentication token via email
func (s *EmailService) SendAdminToken(ctx context.Context, toEmail string, token string, expiresAt time.Time) error {
	if toEmail == "" {
		return fmt.Errorf("recipient email is required")
	}
	if token == "" {
		return fmt.Errorf("token is required")
	}

	subject := "BoomChecker Admin Authentication Token"
	htmlBody, err := s.templates.RenderAdminTokenHTML(token, expiresAt)
	if err != nil {
		return fmt.Errorf("failed to render HTML template: %w", err)
	}

	textBody, err := s.templates.RenderAdminTokenText(token, expiresAt)
	if err != nil {
		return fmt.Errorf("failed to render text template: %w", err)
	}

	if err := s.sendEmail(ctx, toEmail, subject, htmlBody, textBody); err != nil {
		return fmt.Errorf("failed to send admin token email: %w", err)
	}

	log.Printf("Admin token email sent successfully to: %s", toEmail)
	return nil
}

// sendEmail sends an email via AWS SES
func (s *EmailService) sendEmail(ctx context.Context, toEmail, subject, htmlBody, textBody string) error {
	input := &sesv2.SendEmailInput{
		FromEmailAddress: aws.String(s.fromEmail),
		Destination: &types.Destination{
			ToAddresses: []string{toEmail},
		},
		Content: &types.EmailContent{
			Simple: &types.Message{
				Subject: &types.Content{
					Data:    aws.String(subject),
					Charset: aws.String("UTF-8"),
				},
				Body: &types.Body{
					Html: &types.Content{
						Data:    aws.String(htmlBody),
						Charset: aws.String("UTF-8"),
					},
					Text: &types.Content{
						Data:    aws.String(textBody),
						Charset: aws.String("UTF-8"),
					},
				},
			},
		},
	}

	result, err := s.client.SendEmail(ctx, input)
	if err != nil {
		return fmt.Errorf("SES SendEmail failed: %w", err)
	}

	// Log successful send with MessageId for tracking
	if result.MessageId != nil {
		log.Printf("Email sent successfully! AWS SES MessageId: %s", *result.MessageId)
	}

	return nil
}
