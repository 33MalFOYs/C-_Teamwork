-- Test fixture only: install in an isolated expense_tracker_test_* schema.
-- This deliberate server rejection verifies rollback after an earlier row
-- in the same import transaction has already been inserted.
DELIMITER $$
CREATE TRIGGER phase3_reject_test_expense BEFORE INSERT ON expenses
FOR EACH ROW
BEGIN
    IF NEW.category = '__reject_test__' THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Isolated test fixture rejection';
    END IF;
END$$
DELIMITER ;
