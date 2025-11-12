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
)

// EmailService handles email sending via AWS SES
type EmailService struct {
	client    *sesv2.Client
	fromEmail string
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

	return &EmailService{
		client:    client,
		fromEmail: cfg.FromEmail,
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
	htmlBody := s.generateAdminTokenEmailHTML(token, expiresAt)
	textBody := s.generateAdminTokenEmailText(token, expiresAt)

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

	_, err := s.client.SendEmail(ctx, input)
	if err != nil {
		return fmt.Errorf("SES SendEmail failed: %w", err)
	}

	return nil
}

// generateAdminTokenEmailHTML generates HTML email body for admin token
func (s *EmailService) generateAdminTokenEmailHTML(token string, expiresAt time.Time) string {
	expiresInHours := int(time.Until(expiresAt).Hours())

	return fmt.Sprintf(`
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>BoomChecker Admin Token</title>
</head>
<body style="font-family: Arial, sans-serif; line-height: 1.6; color: #333; max-width: 600px; margin: 0 auto; padding: 20px;">
    <div style="background-color: #f8f9fa; padding: 30px; border-radius: 10px;">
        <h2 style="color: #007bff; margin-top: 0;">BoomChecker Admin Authentication</h2>

        <p>Your admin authentication token has been generated.</p>

        <div style="background-color: #fff; padding: 20px; border-radius: 5px; margin: 20px 0; border-left: 4px solid #007bff;">
            <p style="margin: 0 0 10px 0; font-weight: bold;">Your Token:</p>
            <code style="display: block; background-color: #f8f9fa; padding: 15px; border-radius: 5px; word-break: break-all; font-size: 12px; font-family: 'Courier New', monospace;">%s</code>
        </div>

        <div style="background-color: #fff3cd; padding: 15px; border-radius: 5px; border-left: 4px solid #ffc107; margin: 20px 0;">
            <p style="margin: 0; font-weight: bold; color: #856404;">⏰ Token Expiration</p>
            <p style="margin: 5px 0 0 0; color: #856404;">This token will expire in <strong>%d hours</strong> (%s)</p>
        </div>

        <div style="background-color: #fff; padding: 20px; border-radius: 5px; margin: 20px 0;">
            <h3 style="margin-top: 0; color: #28a745;">How to use this token:</h3>
            <ol style="margin: 10px 0; padding-left: 20px;">
                <li>Copy the token above</li>
                <li>Include it in your API requests as a Bearer token</li>
                <li>Add the header: <code style="background-color: #f8f9fa; padding: 2px 6px; border-radius: 3px;">Authorization: Bearer YOUR_TOKEN</code></li>
            </ol>

            <p><strong>Example cURL command:</strong></p>
            <pre style="background-color: #f8f9fa; padding: 15px; border-radius: 5px; overflow-x: auto; font-size: 12px;">curl -H "Authorization: Bearer %s" \
  https://api.boomchecker.com/admin/...</pre>
        </div>

        <div style="background-color: #f8d7da; padding: 15px; border-radius: 5px; border-left: 4px solid #dc3545; margin: 20px 0;">
            <p style="margin: 0; font-weight: bold; color: #721c24;">🔒 Security Notice</p>
            <p style="margin: 5px 0 0 0; color: #721c24;">
                • Keep this token secret and secure<br>
                • Do not share it with anyone<br>
                • You can request a new token only once per 24 hours
            </p>
        </div>

        <hr style="border: none; border-top: 1px solid #dee2e6; margin: 30px 0;">

        <p style="font-size: 12px; color: #6c757d; margin: 0;">
            This is an automated message from BoomChecker API.<br>
            If you did not request this token, please ignore this email.
        </p>
    </div>
</body>
</html>
`, token, expiresInHours, expiresAt.Format("2006-01-02 15:04:05 MST"), token)
}

// generateAdminTokenEmailText generates plain text email body for admin token
func (s *EmailService) generateAdminTokenEmailText(token string, expiresAt time.Time) string {
	expiresInHours := int(time.Until(expiresAt).Hours())

	return fmt.Sprintf(`BoomChecker Admin Authentication

Your admin authentication token has been generated.

YOUR TOKEN:
%s

TOKEN EXPIRATION:
This token will expire in %d hours (%s)

HOW TO USE THIS TOKEN:
1. Copy the token above
2. Include it in your API requests as a Bearer token
3. Add the header: Authorization: Bearer YOUR_TOKEN

EXAMPLE CURL COMMAND:
curl -H "Authorization: Bearer %s" https://api.boomchecker.com/admin/...

SECURITY NOTICE:
• Keep this token secret and secure
• Do not share it with anyone
• You can request a new token only once per 24 hours

---
This is an automated message from BoomChecker API.
If you did not request this token, please ignore this email.
`, token, expiresInHours, expiresAt.Format("2006-01-02 15:04:05 MST"), token)
}
